import os
import platform
from pathlib import Path

import FileUtil

class SetupPremake:
    premakeVersion = "5.0.0-beta2"
    premakeLicenseUrl = "https://raw.githubusercontent.com/premake/premake-core/master/LICENSE.txt"
    premakeDirectory = "./vendor/premake/bin"
    premakeUrls = {
        "Windows": f"https://github.com/premake/premake-core/releases/download/v{premakeVersion}/premake-{premakeVersion}-windows.zip",
        "Linux": f"https://github.com/premake/premake-core/releases/download/v{premakeVersion}/premake-{premakeVersion}-linux.tar.gz",
    } # Platform-specific Premake download URL
    premakeExecutables = {
        "Windows": "premake5.exe",
        "Linux": "premake5"
    } # Platform-specific premake binary

    @classmethod
    def Validate(cls):
        if(not cls.IsPremakeInstalled()):
            print("Premake not found.")
            return cls.InstallPremake()
        
        print(f"Correct Premake found at {os.path.abspath(cls.premakeDirectory)}")
        return True
    
    @classmethod
    def IsPremakeInstalled(cls):
        premakeExecutable = Path(f"{cls.premakeDirectory}/{cls.premakeExecutables[platform.system()]}") # Creates path to where premake5 binary should be
        if(not premakeExecutable.exists()):
            return False
        return True
    
    @classmethod
    def InstallPremake(cls):
        # Prompt user for permission to download premake
        hasPermission = False
        while not hasPermission:
            reply = str(input(f"Would you like to install Premake {cls.premakeVersion}? [Y/n]: ")).lower().strip()[:1]
            if reply == 'n':
                return False
            hasPermission = (reply == 'y' or reply == '')

        url = cls.premakeUrls[platform.system()] # Gets platform-specific premake download url
        premakePath = f"{cls.premakeDirectory}/premake-{cls.premakeVersion}-{platform.system().lower()}"

        if platform.system() == "Windows":
            premakePath += ".zip"
        elif platform.system() == "Linux":
            premakePath += ".tar.gz"

        FileUtil.DownloadFile(url, premakePath)

        # Un-zips or un-tars file appropriately based upon the file extension
        if Path(premakePath).suffix == ".zip":
            FileUtil.ZipFile(premakePath)
        elif Path(premakePath).suffix == ".gz":
            FileUtil.TarFile(premakePath)

        # Gets premake license to avoid legal issues!
        FileUtil.DownloadFile(cls.premakeLicenseUrl, f"{cls.premakeDirectory}/LICENSE.txt")
        return True

if __name__ == "__main__":
    SetupPremake.Validate()