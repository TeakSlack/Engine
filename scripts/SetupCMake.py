import subprocess
import platform

class SetupCMake:
    requiredVersion = (3, 20)

    @classmethod
    def Validate(cls):
        if not cls.IsCMakeInstalled():
            print(f"CMake {cls.requiredVersion[0]}.{cls.requiredVersion[1]}+ not found.")
            cls.PrintInstallInstructions()
            return False
        return True

    @classmethod
    def IsCMakeInstalled(cls):
        try:
            result = subprocess.run(['cmake', '--version'],
                                    capture_output=True, text=True)
            if result.returncode != 0:
                return False

            # First line: "cmake version X.Y.Z"
            parts = result.stdout.splitlines()[0].split()[-1].split('.')
            major, minor = int(parts[0]), int(parts[1])

            if (major, minor) >= cls.requiredVersion:
                print(f"CMake {major}.{minor}.{parts[2]} found.")
                return True

            print(f"CMake {major}.{minor} found, but {cls.requiredVersion[0]}.{cls.requiredVersion[1]}+ is required.")
            return False
        except FileNotFoundError:
            return False

    @classmethod
    def PrintInstallInstructions(cls):
        if platform.system() == "Windows":
            print("Please install CMake from https://cmake.org/download/")
            print("Ensure you select 'Add CMake to the system PATH' during installation.")
        elif platform.system() == "Linux":
            print("Please install CMake via your package manager:")
            print("  sudo apt install cmake       (Debian/Ubuntu)")
            print("  sudo dnf install cmake       (Fedora)")
            print("  sudo pacman -S cmake         (Arch)")

if __name__ == "__main__":
    SetupCMake.Validate()
