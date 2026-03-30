import os
import sys
import platform
import subprocess

from SetupPython import SetupPython
SetupPython.Validate() # Ensure proper packages are installed before using them

from SetupCMake import SetupCMake
from SetupVulkan import SetupVulkan
from SetupProjectsLinux import SetupProjectsLinux

# Change directory to project root
os.chdir('../')

hasCMake = SetupCMake.Validate()
SetupVulkan.Validate()

if not hasCMake:
    print("CMake not installed.")
    sys.exit(1)

print("Running CMake configure...")

if platform.system() == "Windows":
    subprocess.call([
        'cmake', '-S', '.', '-B', 'build',
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64'
    ])
elif platform.system() == "Linux":
    SetupProjectsLinux.Setup()
