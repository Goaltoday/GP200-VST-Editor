@echo off
setlocal
cd /d "%~dp0"

if not exist "external\JUCE\CMakeLists.txt" if not exist "JUCE\CMakeLists.txt" if "%JUCE_SOURCE_DIR%"=="" (
    echo JUCE was not found beside the project.
    echo Set JUCE_SOURCE_DIR or copy JUCE to external\JUCE.
    exit /b 1
)

set CMAKE_JUCE_ARG=
if not "%JUCE_SOURCE_DIR%"=="" set CMAKE_JUCE_ARG=-DJUCE_SOURCE_DIR=%JUCE_SOURCE_DIR%

cmake -S . -B build %CMAKE_JUCE_ARG%
if errorlevel 1 exit /b %errorlevel%

cmake --build build --config Debug
exit /b %errorlevel%
