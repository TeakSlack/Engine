#!/bin/bash

if command -v python3 &>/dev/null; then
    echo "Python found."
    python3 ./Setup.py
else
    echo "Python has either not been installed or cannot be found in your system's PATH."
    echo "Please install Python 3.3 or greater or add it to your system's PATH."
    exit 1
fi