@echo off
REM Start the dual EOM GUI without a console (uses eom.ini here if present).
setlocal
pushd "%~dp0"
if not exist ".venv\Scripts\pythonw.exe" (
    echo Virtual environment not found. Run setup_env.bat first.
    pause
    exit /b 1
)
start "" ".venv\Scripts\pythonw.exe" -m eom_controller %*
popd
endlocal
