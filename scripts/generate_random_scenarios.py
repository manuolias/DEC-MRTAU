#!/usr/bin/env python3
"""
Generates 45 random scenario YAML files in scenarios/random/.

Structure:
  - 15 base scenarios: 5x10 tasks, 5x15 tasks, 5x20 tasks
  - Each base scenario x 3 robot variants (3, 4, 5 robots)
  = 45 files total
"""

import random
import os

SEED = 2025
random.seed(SEED)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "scenarios", "random")

TIME_WINDOW_OPTIONS = [270, 360, 450, 540, 630, 720, 810, 900, 1080]


def generate_unique_coords(n: int) -> list[tuple[int, int]]:
    """Generate n unique integer coordinate pairs in [-100,100]^2, excluding (0,0)."""
    used = {(0, 0)}
    result = []
    while len(result) < n:
        x = random.randint(-100, 100)
        y = random.randint(-100, 100)
        if (x, y) not in used:
            used.add((x, y))
            result.append((x, y))
    return result


def flow_list(items) -> str:
    return "[" + ",".join(str(i) for i in items) + "]"


def write_scenario(path: str, name: str, task_coords: list, tasks_data: list, robot_count: int):
    num_tasks = len(tasks_data)
    num_nodes = num_tasks + 1  # node 1 (depot) + one per task
    all_node_ids = list(range(1, num_nodes + 1))
    all_task_ids = list(range(1, num_tasks + 1))

    lines = []
    lines.append(f'name: "{name}"')
    lines.append("height: 200")
    lines.append("width: 200")
    lines.append("initial_time: 0")
    lines.append("")

    # Graph
    lines.append("graph:")
    # Node 1 — depot at (0,0)
    lines.append(f"  - node: 1")
    lines.append(f'    description: "Node 1"')
    lines.append(f"    coords: [0,0]")
    lines.append(f"    neighbors: {flow_list(all_node_ids)}")
    # Task nodes
    for i, (x, y) in enumerate(task_coords):
        nid = i + 2
        lines.append(f"  - node: {nid}")
        lines.append(f'    description: "Node {nid}"')
        lines.append(f"    coords: [{x},{y}]")
        lines.append(f"    neighbors: {flow_list(all_node_ids)}")
    lines.append("")

    # Tasks
    lines.append("tasks:")
    for t in tasks_data:
        tid = t["id"]
        nid = tid + 1  # task i lives at node i+1
        tw = t["time_window"]
        lines.append(f"  - id: {tid}")
        lines.append(f"    node: {nid}")
        lines.append(f'    description: "Task {tid}"')
        lines.append(f"    time_window: {flow_list(tw)}")
        lines.append(f"    success_prob: {t['success_prob']}")
        lines.append(f"    success_time: [10,90]")
        lines.append(f"    fail_time: [0,0]")
        lines.append(f"    demand: [10,10]")
        lines.append(f"    required_workers: {t['required_workers']}")
    lines.append("")

    # Robots
    lines.append("robots:")
    for r in range(1, robot_count + 1):
        lines.append(f"  - id: {r}")
        lines.append(f"    initial_node: 1")
        lines.append(f'    description: "Robot {r}"')
        lines.append(f"    initial_battery_level: 100")
        lines.append(f"    battery_capacity: 100")
        lines.append(f"    navigation_velocity: 5")
        lines.append(f"    battery_rate_while_navigating: 0.1")
        lines.append(f"    capabilities: {flow_list(all_task_ids)}")
    lines.append("")

    # Recharging stations
    lines.append("recharging_stations:")
    lines.append(f"  - id: 1")
    lines.append(f"    node: 1")
    lines.append(f'    description: "Station 1"')
    lines.append("")

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(lines))


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    generated = []

    task_group_sizes = [10, 15, 20]

    for num_tasks in task_group_sizes:
        for scenario_num in range(1, 6):
            # Generate node coordinates once per base scenario
            coords = generate_unique_coords(num_tasks)

            # Generate task properties once per base scenario
            tasks_data = []
            for tid in range(1, num_tasks + 1):
                tasks_data.append({
                    "id": tid,
                    "time_window": [0, random.choice(TIME_WINDOW_OPTIONS)],
                    "success_prob": round(random.uniform(0.5, 0.9), 1),
                    "required_workers": random.randint(1, 3),
                })

            # Write one file per robot variant
            for robot_count in [3, 4, 5]:
                name = f"scenario_{num_tasks}t_{scenario_num:02d}_{robot_count}r"
                filename = f"{name}.yaml"
                path = os.path.join(OUTPUT_DIR, filename)
                write_scenario(path, name, coords, tasks_data, robot_count)
                generated.append(filename)
                print(f"  Generated: {filename}")

    print(f"\nTotal: {len(generated)} scenarios written to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
