@echo off
setlocal

set "PROJECT_ROOT=%~dp0..\"
set ENGINE_PATH="C:\Users\Unreal\UE_5.5"

:: —— 删除目录 —— 
for %%D in (Intermediate Binaries DerivedDataCache Saved) do (
    if exist "%PROJECT_ROOT%%%D" (
        echo Deleting "%PROJECT_ROOT%%%D"...
        rd /s /q "%PROJECT_ROOT%%%D"
    )
)

echo Deleted [Intermediate, Binaries, DerivedDataCache, Saved]
endlocal
pause
