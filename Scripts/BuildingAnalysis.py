# BuildingAnalysis.py

import json
import math
import pandas as pd
import numpy as np
import networkx as nx

# 配置
BASE_Z = 10.0         # 地面中心高度
FLOOR_HEIGHT = 300.0  # 每层高度
EXTEND = 50.0         # 墙段前后延伸量

def load_records(json_path):
    with open(json_path, 'r') as f:
        return json.load(f)

def detect_wall_cycles(records, layer=0):
    # 筛出墙体
    walls = [r for r in records if 'Wall' in r['ClassPath']]
    edges = []
    for w in walls:
        x = w['Translation']['X']
        y = w['Translation']['Y']
        z = w['Translation']['Z']
        # 只看指定层
        this_layer = math.floor((z - BASE_Z) / FLOOR_HEIGHT)
        if this_layer != layer:
            continue

        # 原来直接用 yaw；现在 +90°，变成墙体延伸方向
        yaw_deg = w['Rotation']['Yaw'] + 90.0
        yaw = math.radians(yaw_deg)
        dx = math.cos(yaw)
        dy = math.sin(yaw)

        # 墙段中心点前后各延伸 EXTEND
        p1 = (round(x - dx * EXTEND, 2), round(y - dy * EXTEND, 2))
        p2 = (round(x + dx * EXTEND, 2), round(y + dy * EXTEND, 2))
        edges.append((p1, p2))

    G = nx.Graph()
    G.add_edges_from(edges)

    # 找所有基础环路
    cycles = nx.cycle_basis(G)
    return cycles

def main():
    records = load_records('../Saved/structure_records.json')

    # 1) 基础统计
    df = pd.json_normalize(records)
    summary = df['ClassPath'].value_counts().rename_axis('Blueprint').reset_index(name='Count')
    print("=== 各类型构件数量 ===")
    print(summary.to_string(index=False))

    # 2) 检测地面层（layer=0）的环路
    cycles = detect_wall_cycles(records, layer=0)
    print(f"\n地面层（z≈{BASE_Z}）检测到闭合环路（“房间”）数量：{len(cycles)}")
    for i, cycle in enumerate(cycles, 1):
        print(f"  房间 {i}：顶点 {cycle}")

    # 3) 可能的屋顶插槽数 = 基座数量
    foundations = [r for r in records if 'Foundation' in r['ClassPath']]
    print(f"\n可能的屋顶插槽总数：{len(foundations)}")

    # 4) 导出一份 summary.csv
    summary.to_csv('structure_summary.csv', index=False)
    print("\n已生成：structure_summary.csv")

if __name__ == '__main__':
    main()
