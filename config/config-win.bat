@echo OFF
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

set INITIAL_SAVED_DIR=%CD%

REM Create build dir and switch to it
cd ..
mkdir build
mkdir build\win-%ARCH%
cd build\win-%ARCH%

REM Use CMAKE to generate project
cmake -G "Visual Studio 17 2022" -A %ARCH% ..\..\Project

cd /d "%INITIAL_SAVED_DIR%"
cd ..

REM Prebuild every engine project so user can skip this.
echo Try to build all necesary engine projects
cmake --build build\win-%ARCH% --target BuildAllEngine
echo All engine builds complete!

REM Prebuild every project subprojects so user can skip this.
echo Try to build all necesary projects
cmake --build build\win-%ARCH% --target BuildAllProject
echo All builds complete!

REM 
if "%CI%"=="true" (
    echo Running in CI - skipping VS open and pause
) else (
	REM Finally open Visual Studio
	REM Note: You could change this part to IDE of your choice
	echo try to open Visual studio
    cmake --build build\win-%ARCH% --target open_vs
	echo Visual studio show now open
    PAUSE
)
