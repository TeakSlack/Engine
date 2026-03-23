import sys
import subprocess
import platform
import importlib.util as importlib_util

class SetupPython:
    @classmethod
    def Validate(cls):
        if not cls.ValidatePython():
            return
        
        if cls.ValidatePip():
            cls.ValidatePackage('tqdm')
            cls.ValidatePackage('requests')
            return
        else:
            print("Pip is unavailable. Please install pip.")
            exit()


    @classmethod
    def ValidatePython(cls, versionMajor = 3, versionMinor = 3):
        v = sys.version_info
        if sys.version is None:
            return False
        print(f"Python version {v.major}.{v.minor}.{v.micro} found.")
        if v.major < versionMajor or (v.major == versionMajor and v.minor < versionMinor):
            print(f"Python version too low, version {versionMajor}.{versionMinor} or higher is required.")
            return False
        return True
    
    @classmethod
    def ValidatePackage(cls, packageName):
        if importlib_util.find_spec(packageName) is None: # If package isn't found, attempt to install it
            print(f"Package {packageName} not found.")
            return cls.InstallPackage(packageName)
        return True
    
    @classmethod
    def ValidatePip(cls):
        if importlib_util.find_spec('pip') is None:
            return False
        return True

    @classmethod
    def InstallPackage(cls, packageName):
        # Sets python command to be used based on system
        pythonExec = ""
        if platform.system() == "Windows":
            pythonExec = "python"
        elif platform.system() == "Linux":
            pythonExec = "python3"

        # Prompts user for permission to install package
        hasPermission = False
        while not hasPermission:
            reply = str(input(f"Would you like to install Python package {packageName}? [Y/n]: ")).lower().strip()[:1]
            if reply == 'n':
                return False
            hasPermission = (reply == 'y' or reply == '')

        print(f"Installing package {packageName}...")
        subprocess.check_call([pythonExec, '-m', 'pip', 'install', packageName])

        return cls.ValidatePackage(packageName) # Checks if package was installed successfully 
    
if __name__ == "__main__":
    SetupPython.Validate()