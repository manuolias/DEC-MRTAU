#!/usr/bin/env bash
#
# Ejecuta el catálogo de escenarios repartiéndolo entre varios procesos.
#
#   scripts/run_catalog.sh [n_procesos] [dir_escenarios] [dir_logs] [config]
#
# El ejecutable es de un solo hilo, así que la paralelización se hace repartiendo
# los escenarios en «shards» (directorios con enlaces simbólicos) y lanzando un
# proceso por shard. Todos escriben en el mismo directorio de logs; como cada run
# produce un fichero distinto, no hay conflicto de escritura.
#
# El ejecutor es idempotente (salta los .log ya completos), así que la tanda se
# puede interrumpir y relanzar sin perder trabajo.
#
# ⚠️ En paralelo, la métrica `computing_time` deja de ser comparable entre tandas
#    por la contención de CPU. Si se necesita esa métrica, relanzar en serie.

set -euo pipefail

WORKERS=${1:-4}
SCEN_DIR=$(realpath "${2:-$(dirname "$0")/../scenarios/catalogo}")
LOGS_DIR=$(realpath -m "${3:-$(dirname "$0")/../logs/eval_catalogo}")
CONFIG=$(realpath "${4:-$SCEN_DIR/experiment_config.yaml}")
BIN=$(realpath "$(dirname "$0")/../build/simulador")
SHARD_ROOT="${LOGS_DIR}/.shards"

[[ -x "$BIN" ]] || { echo "No existe el ejecutable $BIN (compila con cmake/make)"; exit 1; }

mkdir -p "$LOGS_DIR"
rm -rf "$SHARD_ROOT"
mkdir -p "$SHARD_ROOT"

# Reparto round-robin de los escenarios entre los shards
mapfile -t FILES < <(find "$SCEN_DIR" -maxdepth 1 -name '*.yaml' ! -name 'experiment_config.yaml' | sort)
echo "Escenarios: ${#FILES[@]} | procesos: $WORKERS | logs: $LOGS_DIR"

for ((i = 0; i < ${#FILES[@]}; i++)); do
    shard=$(printf "%02d" $((i % WORKERS)))
    mkdir -p "$SHARD_ROOT/$shard"
    ln -sf "${FILES[$i]}" "$SHARD_ROOT/$shard/"
done

pids=()
for ((w = 0; w < WORKERS; w++)); do
    shard=$(printf "%02d" "$w")
    "$BIN" "$SHARD_ROOT/$shard" "$LOGS_DIR" "$CONFIG" > "$LOGS_DIR/.worker_$shard.out" 2>&1 &
    pids+=($!)
done

echo "Lanzados ${#pids[@]} procesos. Progreso:  tail -f $LOGS_DIR/.worker_00.out"

fail=0
for pid in "${pids[@]}"; do wait "$pid" || fail=1; done

rm -rf "$SHARD_ROOT"
if [[ $fail -eq 0 ]]; then
    echo "--- Tanda completa: $(find "$LOGS_DIR" -name '*.log' | wc -l) logs en $LOGS_DIR ---"
else
    echo "--- Algún proceso terminó con error; revisa $LOGS_DIR/.worker_*.out ---"
    exit 1
fi
