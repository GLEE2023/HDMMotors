@echo off
setlocal
cd /d "%~dp0"
py -m pip install -r requirements.txt pyinstaller || exit /b 1
py -m PyInstaller --clean --noconfirm --onefile --windowed --name GLEE-Motor-Studio --collect-submodules PySide6.QtSerialPort run.py || exit /b 1
echo Executable available at dist\GLEE-Motor-Studio.exe
endlocal
