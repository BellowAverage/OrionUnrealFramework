import os
import sys
import subprocess
import unreal

# 1. Locate the current project file and the editor executable
uproject = unreal.Paths.get_project_file_path()                   # e.g. "C:/MyGame/MyGame.uproject" :contentReference[oaicite:0]{index=0}
engine_dir = unreal.Paths.engine_dir()                            # e.g. "C:/Program Files/Epic Games/UE_5.5/" :contentReference[oaicite:1]{index=1}
editor_exe = os.path.join(engine_dir, "Binaries", "Win64", "UnrealEditor.exe")

# 2. Spawn a new editor process
subprocess.Popen([editor_exe, uproject])

# 3. Quit the current editor cleanly
#    There’s no direct Python API call, but you can send the console 'quit' command:
world = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_editor_world()  # any valid world context :contentReference[oaicite:2]{index=2}
unreal.SystemLibrary.execute_console_command(world, "quit")                      # blueprint node mapped in Python
