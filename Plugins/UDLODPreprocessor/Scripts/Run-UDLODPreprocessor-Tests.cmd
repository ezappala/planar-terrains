@echo off
setlocal

set "PROJECT=C:\Users\ellie\dev\PlanarTerrains\PlanarTerrains.uproject"
set "ENGINE=C:\Program Files\Epic Games\UE_5.7\Engine"
set "PLUGIN_ROOT=%~dp0.."
set "MODE=%~1"
set "TEST_FILTER=%~2"

if "%TEST_FILTER%"=="" set "TEST_FILTER=UDLODPreprocessor"

if /I "%MODE%"=="coverage" (
    set "LOG_PATH=%PLUGIN_ROOT%\Saved\Coverage\UDLODPreprocessor-Automation.log"
) else (
    set "LOG_PATH=%PLUGIN_ROOT%\Saved\TestResults\UDLODPreprocessor-Automation.log"
)

if not exist "%PLUGIN_ROOT%\Saved\TestResults" mkdir "%PLUGIN_ROOT%\Saved\TestResults"
if not exist "%PLUGIN_ROOT%\Saved\Coverage" mkdir "%PLUGIN_ROOT%\Saved\Coverage"

"%ENGINE%\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT%" ^
    -unattended ^
    -nop4 ^
    -nosplash ^
    -NullRHI ^
    -stdout ^
    -FullStdOutLogOutput ^
    "-ExecCmds=Automation RunTests %TEST_FILTER%;Quit" ^
    "-TestExit=Automation Test Queue Empty" ^
    "-log=%LOG_PATH%"

exit /b %ERRORLEVEL%
