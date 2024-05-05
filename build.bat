@echo off
rem This script is part of MyBCP
rem Purpose of this script is to ensure python is installed, so we can elevate into a more sane language.
rem Also, we install conan/cmake through python, because that's way easier than doing all this weird dancing around, like we do for python.

rem CodePage 65001 is UTF-8, IDK what you're doing but everything's kinda UTF-8, so switch
chcp 65001
setlocal
where /Q python3
if not ERRORLEVEL 1 goto run

set "pyversion=3.12"
echo  ====== Installing Python %pyversion% ======
where /Q winget
if ERRORLEVEL 1 (goto py_manual)

:py_store
echo installing though winget
winget install --id Python.Python.%pyversion%
goto run

:py_manual
echo installing manually winget
set "pyfilearch="
if "%PROCESSOR_ARCHITECTURE%" EQU "ARM64" (
  set "pyfilearch=-arm64"
  goto py_download
)
if "%PROCESSOR_ARCHITECTURE:~2,2%" EQU "64" (
  set "pyfilearch=-amd64"
  goto py_download
)
if "%PROCESSOR_ARCHITECTURE:~3,2%" EQU "64" (
  set "pyfilearch=-amd64"
  goto py_download
)
if "%PROCESSOR_ARCHITECTURE%" NEQ "X86" (
  echo "Unknown arch %PROCESSOR_ARCHITECTURE%"
  goto end
)

:py_download
curl -o python_setup.exe https://www.python.org/ftp/python/%pyversion%.0/python-%pyversion%.0%pyfilearch%.exe
if ERRORLEVEL 1 (
  echo "Failed to download python installer"
  goto end
)
python_setup.exe /passive AppendPath=1
if ERRORLEVEL 1 (
  echo "Installer failed/cancelled"
  goto end
)
if exist python_setup.exe (del python_setup.exe)
rem Note: we test for/use python3 here, but the venv only knows python! (should be python3 then tho)
where /Q python3
if ERRORLEVEL 1 (
  echo "Python is not yet visible, please restart the script/cmd/computer"
  goto end
)

:run
set "scriptdir=%~dp0"
cd %scriptdir%
rem Activate venv
if exist venv_build (goto venv)
echo  ====== Creating Python Virtual Environment ======
echo    NOTE: Conan will cache packages in %HOMEDRIVE%%HOMEPATH%\.conan2 by default
python3 -m venv venv_build
if ERRORLEVEL 1 (goto end)

:venv
rem Run script in venv, passing all args
cd %scriptdir%
call venv_build\Scripts\activate
cd %scriptdir%
call python build_py %*
call deactivate

:end
endlocal
