import unreal
import json
import os

def export_static_mesh_actors_to_json(file_path):
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    data = []
    
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue

        actor_name = actor.get_name()
        actor_class = actor.get_class().get_name()
        actor_path = actor.get_path_name()
        
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()

        actor_data = {
            "name": actor_name,
            "class": actor_class,
            "path": actor_path,
            "location": {"x": location.x, "y": location.y, "z": location.z},
            "rotation": {"pitch": rotation.pitch, "yaw": rotation.yaw, "roll": rotation.roll},
            "scale": {"x": scale.x, "y": scale.y, "z": scale.z}
        }
        data.append(actor_data)

    output_dir = os.path.dirname(file_path)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(file_path, 'w') as f:
        json.dump(data, f, indent=4)
    unreal.log("Level data exported successfully to: " + file_path)

project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
data_folder = os.path.join(project_dir, "Data")
output_path = os.path.join(data_folder, "level_data.json")

export_static_mesh_actors_to_json(output_path)
