# BuildingAnalysis.py

import json
import math
import pandas as pd
import networkx as nx
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon

# —— 配置区 —— #
JSON_PATH    = '../Saved/structure_records.json'
BASE_Z       = 10.0     # 地面中心高度
FLOOR_HEIGHT = 300.0    # 每层高度
EXTEND       = 50.0     # 墙段前后延伸量
CSV_OUT      = 'structure_summary.csv'

def load_records(json_path):
    with open(json_path, 'r') as f:
        return json.load(f)

def detect_wall_cycles(records, layer=0):
    walls = [r for r in records if 'BP_OrionStructureWall' in r['ClassPath']]
    edges = []
    for w in walls:
        x, y, z = (w['Translation'][k] for k in ('X','Y','Z'))
        if math.floor((z - BASE_Z) / FLOOR_HEIGHT) != layer:
            continue

        # +90°：把法线旋转到“沿墙延伸”方向
        yaw = math.radians(w['Rotation']['Yaw'] + 90.0)
        dx, dy = math.cos(yaw), math.sin(yaw)

        # 用整点来保证节点对齐
        p1 = (int(round(x - dx * EXTEND)), int(round(y - dy * EXTEND)))
        p2 = (int(round(x + dx * EXTEND)), int(round(y + dy * EXTEND)))
        edges.append((p1, p2))

    G = nx.Graph()
    G.add_edges_from(edges)
    return nx.cycle_basis(G)

def plot_walls_and_cycles(records, cycles, layer=0):
    walls = []
    for r in records:
        if 'BP_OrionStructureWall' not in r['ClassPath']:
            continue
        x, y, z = (r['Translation'][k] for k in ('X','Y','Z'))
        if math.floor((z - BASE_Z) / FLOOR_HEIGHT) != layer:
            continue

        yaw = math.radians(r['Rotation']['Yaw'] + 90.0)
        dx, dy = math.cos(yaw), math.sin(yaw)
        p1 = (int(round(x - dx * EXTEND)), int(round(y - dy * EXTEND)))
        p2 = (int(round(x + dx * EXTEND)), int(round(y + dy * EXTEND)))
        walls.append((p1, p2))

    fig, ax = plt.subplots(figsize=(10,8))
    # all walls
    for p1, p2 in walls:
        ax.plot([p1[0],p2[0]],[p1[1],p2[1]], color='gray', lw=1)

    # draw cycles
    cmap = plt.get_cmap('tab10')
    for idx, cycle in enumerate(cycles):
        poly = Polygon(cycle, closed=True,
                       facecolor=cmap(idx%10), edgecolor='black', alpha=0.3)
        ax.add_patch(poly)
        # centroid
        cx = sum(x for x,y in cycle)/len(cycle)
        cy = sum(y for x,y in cycle)/len(cycle)
        ax.text(cx, cy, str(idx+1), ha='center', va='center',
                fontsize=12, fontweight='bold')

    ax.set_title(f'Wall Segments & Detected Cycles (Layer {layer})')
    ax.set_aspect('equal','box')
    ax.set_xlabel('X'); ax.set_ylabel('Y')
    plt.tight_layout()
    plt.show()

def main():
    records = load_records(JSON_PATH)

    # 1) 基础统计
    df = pd.json_normalize(records)
    summary = ( df['ClassPath']
                .value_counts()
                .rename_axis('Blueprint')
                .reset_index(name='Count') )
    print("=== 各类型构件数量 ===")
    print(summary.to_string(index=False))
    summary.to_csv(CSV_OUT, index=False)
    print(f"\n已导出统计到：{CSV_OUT}")

    # 2) 检测并打印房间数
    cycles = detect_wall_cycles(records, layer=0)
    print(f"\n地面层检测到房间数量：{len(cycles)}")
    for i, cyc in enumerate(cycles, 1):
        print(f"  房间 {i} 顶点：{cyc}")

    # 3) 屋顶插槽数
    roofs = [r for r in records if 'Foundation' in r['ClassPath']]
    print(f"\n可能的屋顶插槽总数：{len(roofs)}")

    # 4) 画图
    plot_walls_and_cycles(records, cycles, layer=0)

if __name__=='__main__':
    main()
