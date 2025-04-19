import unreal
import json
import os

def export_static_mesh_actors_to_json(file_path):
    # 获取当前关卡中的所有 Actor
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    data = []
    
    for actor in actors:
        # 只处理静态网格体类型的 Actor
        if not isinstance(actor, unreal.StaticMeshActor):
            continue

        # 获取 Actor 的基本信息和 Transform 信息
        actor_name = actor.get_name()
        # 获取 Actor 的类型名称（例如蓝图类名称或者 C++ 类名称）
        actor_class = actor.get_class().get_name()
        # 获取 Actor 的引用路径，用于唯一标识（在加载时可以作为查找依据）
        actor_path = actor.get_path_name()
        
        location = actor.get_actor_location()   # FVector
        rotation = actor.get_actor_rotation()   # Rotator
        scale = actor.get_actor_scale3d()         # FVector

        # 将所有数据转换为字典格式
        actor_data = {
            "name": actor_name,
            "class": actor_class,
            "path": actor_path,
            "location": {"x": location.x, "y": location.y, "z": location.z},
            "rotation": {"pitch": rotation.pitch, "yaw": rotation.yaw, "roll": rotation.roll},
            "scale": {"x": scale.x, "y": scale.y, "z": scale.z}
        }
        data.append(actor_data)

    # 如果目录不存在，创建目录
    output_dir = os.path.dirname(file_path)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # 将数据写入 JSON 文件
    with open(file_path, 'w') as f:
        json.dump(data, f, indent=4)
    unreal.log("Level data exported successfully to: " + file_path)

# 获取项目根目录，并指定 Data 文件夹作为存储路径
project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
data_folder = os.path.join(project_dir, "Data")
output_path = os.path.join(data_folder, "level_data.json")

export_static_mesh_actors_to_json(output_path)
