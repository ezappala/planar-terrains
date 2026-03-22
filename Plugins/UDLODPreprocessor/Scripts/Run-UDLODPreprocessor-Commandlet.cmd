@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..\..") do set "REPO_ROOT=%%~fI"

set "UPROJECT=%REPO_ROOT%\PlanarTerrains.uproject"
set "EDITOR_CMD=C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%EDITOR_CMD%" (
    echo UnrealEditor-Cmd.exe not found at "%EDITOR_CMD%"
    exit /b 1
)

"%EDITOR_CMD%" "%UPROJECT%" -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput -run=UDLODPreprocessor %*
exit /b %ERRORLEVEL%
