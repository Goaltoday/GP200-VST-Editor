@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release --target GP200Plugin_VST3 GP200Plugin_Standalone --parallel
if errorlevel 1 exit /b 1
echo VST3: build\GP200Plugin_artefacts\Release\VST3\GP200 VST.vst3
echo EXE:  build\GP200Plugin_artefacts\Release\Standalone\GP200 VST.exe
endlocal
