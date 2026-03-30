import os
import platform
import sys
import urllib.request

import FileUtil

class SetupVulkan:
    requiredVulkanVersion = "1.3"
    vulkanDirectory = "./vendor/VulkanSDK"

    _latestVersionCache = None

    @classmethod
    def GetLatestVersion(cls):
        if cls._latestVersionCache:
            return cls._latestVersionCache
        try:
            url = "https://vulkan.lunarg.com/sdk/latest/windows.txt" if platform.system() == "Windows" \
                else "https://vulkan.lunarg.com/sdk/latest/linux.txt"
            with urllib.request.urlopen(url, timeout=5) as response:
                cls._latestVersionCache = response.read().decode().strip()
        except Exception:
            cls._latestVersionCache = "1.3.296.0"  # fallback
        return cls._latestVersionCache

    @classmethod
    def GetVulkanUrls(cls):
        version = cls.GetLatestVersion()
        return {
            "Windows": f"https://sdk.lunarg.com/sdk/download/{version}/windows/VulkanSDK-{version}-Installer.exe",
            "Linux": f"https://sdk.lunarg.com/sdk/download/{version}/linux/vulkansdk-linux-x86_64-{version}.tar.xz"
        }

    @classmethod
    def Validate(cls):
        return cls.ValidateVulkan()
    
    @classmethod
    def ValidateVulkan(cls):
        vulkanSdk = os.environ.get('VULKAN_SDK') # Checks if VULKAN_SDK is in PATH
        latestVersion = cls.GetLatestVersion()
        vulkanLinuxPath = f"${cls.vulkanDirectory}/{latestVersion}/x86_64" # Checks if Vulkan is installed locally by setup
        if os.path.isdir(vulkanLinuxPath):
            vulkanSdk = os.path.abspath(vulkanLinuxPath)
        if vulkanSdk is None:
            print("Vulkan SDK is not installed!")
            cls.InstallVulkan()
            return False
        if cls.requiredVulkanVersion not in vulkanSdk:
            print(f"Vulkan SDK is out of date! Required version is <{cls.requiredVulkanVersion}")
            return False
        print(f"Valid Vulkan SDK found at {vulkanSdk}")
        return True

    @classmethod
    def InstallVulkan(cls):
        latestVersion = cls.GetLatestVersion()
        # Prompts user for permission to download Vulkan SDK
        hasPermission = False
        while not hasPermission:
            reply = str(input(f"Would you like to install Vulkan {latestVersion} (this may take a while)? [Y/n]: ")).lower().strip()[:1]
            if reply == 'n':
                return False
            hasPermission = (reply == 'y' or reply == '')

        url = cls.GetVulkanUrls()[platform.system()]
        vulkanPath = f"{cls.vulkanDirectory}/vulkan-{latestVersion}-{platform.system().lower()}"

        if platform.system() == "Windows":
            vulkanPath += ".exe"
        elif platform.system() == "Linux":
            vulkanPath += ".tar.xz"

        FileUtil.DownloadFile(url, vulkanPath)

        if platform.system() == "Windows":
            os.startfile(os.path.abspath(vulkanPath)) # Runs Vulkan SDK installer
            print("Re-open terminal and re-run this script after installation.")
            sys.exit(0)
        elif platform.system() == "Linux":
            FileUtil.TarFile(vulkanPath, type='xz')