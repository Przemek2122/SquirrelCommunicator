@echo OFF
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

set INITIAL_SAVED_DIR=%CD%

REM Create build dir and switch to it
cd ..
mkdir buildsrv
mkdir buildsrv\win-%ARCH%
cd buildsrv\win-%ARCH%

REM Use CMAKE to generate ProjectServer
cmake -G "Visual Studio 17 2022" -A %ARCH% ..\..\ProjectServer

cd /d "%INITIAL_SAVED_DIR%"
cd ..

REM Prebuild every engine ProjectServer so user can skip this.
echo Try to build all necesary engine projects
cmake --build buildsrv\win-%ARCH% --target BuildAllEngine --parallel
echo All engine builds complete!

REM Prebuild every ProjectServer subprojects so user can skip this.
echo Try to build all necesary projects
cmake --build buildsrv\win-%ARCH% --target BuildAllProject --parallel
echo All builds complete!

REM IDE Selection (if not CLI)
if "%CI%"=="true" (
    echo Running in CI - skipping IDE open and pause
) else (
    echo Select IDE to open:
    echo 1. Visual Studio
    echo 2. CLion
    echo 3. None
    choice /C 123 /N /M "Enter choice (1-3): "
    
    if errorlevel 3 goto skip_ide
    if errorlevel 2 goto open_clion
    if errorlevel 1 goto open_vs
    
    :open_vs
    echo Opening Visual Studio...
    cmake --build buildsrv\win-%ARCH% --target open_vs
    echo Visual Studio should now be open
    goto end_ide
    
    :open_clion
    echo Opening CLion...
    start "" "clion64" "%CD%/ProjectServer/CMakeLists.txt"
    goto end_ide
    
    :skip_ide
    echo Skipping IDE open
    
    :end_ide
    PAUSE
)
