@echo off
REM Create .venv in this folder and install eom_controller with its GUI.
setlocal
pushd "%~dp0"
py -3 -m venv .venv || (echo Failed to create virtual environment. & pause & exit /b 1)
".venv\Scripts\python.exe" -m pip install --upgrade pip
".venv\Scripts\python.exe" -m pip install -e .[gui] || (echo pip install failed. & pause & exit /b 1)
echo Done. Start the GUI with start_gui.bat
popd
endlocal
