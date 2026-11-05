@echo off
chcp 65001 >nul
cd /d "%~dp0"
set "LOG=%~dp0BUILD_VER101.log"
set "VS=C:\Program Files\Microsoft Visual Studio\2022\Community"
echo ==== BUILD START %DATE% %TIME% ==== > "%LOG%"
echo Initializing Visual Studio...
if not exist "%VS%\Common7\Tools\VsDevCmd.bat" (
  echo ERROR: VsDevCmd.bat not found.>>"%LOG%"
  goto END
)
call "%VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 (
  echo ERROR: Visual Studio environment setup failed.>>"%LOG%"
  goto END
)
echo Running MSBuild...
msbuild "%~dp0BonDriver_Friio_x64.sln" /m /p:Configuration=Release /p:Platform=x64 /verbosity:normal > "%LOG%" 2>&1
set RC=%ERRORLEVEL%
type "%LOG%"
echo.>>"%LOG%"
echo MSBUILD_EXIT_CODE=%RC%>>"%LOG%"
if not "%RC%"=="0" (
  echo.
  echo BUILD FAILED. See BUILD_VER101.log.
) else (
  echo.
  echo BUILD SUCCEEDED: bin\Release\BonDriver_Friio_x64.dll
)
:END
echo.
echo Log file: "%LOG%"
echo Press any key to close.
pause >nul
