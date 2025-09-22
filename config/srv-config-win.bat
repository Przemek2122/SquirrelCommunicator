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
cmake --build buildsrv\win-%ARCH% --target BuildAllEngine
echo All engine builds complete!

REM Prebuild every ProjectServer subprojects so user can skip this.
echo Try to build all necesary projects
cmake --build buildsrv\win-%ARCH% --target BuildAllProject
echo All builds complete!

REM 
if "%CI%"=="true" (
    echo Running in CI - skipping VS open and pause
) else (
	REM Finally open Visual Studio
	REM Note: You could change this part to IDE of your choice
	echo try to open Visual studio
    cmake --build buildsrv\win-%ARCH% --target open_vs
	echo Visual studio show now open
    PAUSE
)
