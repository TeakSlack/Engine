@echo off
setlocal

python --version >nul 2>&1

if %errorlevel%==0 (
    echo Python found.
    python Setup.py
) else (
    echo Python has either not been installed or cannot be found in your system's PATH.
    echo Please install Python 3.3 or greater or add it to your system's PATH.
)

endlocal
PAUSE