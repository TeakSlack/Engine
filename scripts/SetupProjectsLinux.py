import os
import subprocess

class SetupProjectsLinux:
    vulkanSdk = os.environ.get("VULKAN_SDK")
    localVulkanSdk = os.path.abspath('../vendor/VulkanSDK/1.3.296.0/x86_64')

    @classmethod
    def Setup(cls):
        cls.GetVulkan()
        subprocess.call([
            'cmake', '-S', '.', '-B', 'build',
            '-G', 'Unix Makefiles',
            '-DCMAKE_BUILD_TYPE=Debug'
        ])

    @classmethod
    def GetVulkan(cls):
        # Check for local Vulkan installation made by setup
        if cls.vulkanSdk is None:
            if os.path.isdir(cls.localVulkanSdk):
                os.environ["VULKAN_SDK"] = cls.localVulkanSdk
            else:
                print("The Vulkan SDK could not be found on your system! Please install it via the setup or your system's package manager.")
                exit()

if __name__ == "__main__":
    os.chdir('../')
    SetupProjectsLinux.Setup()
