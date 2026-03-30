#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

#include "RenderDevice.h"

#include <nvrhi/vulkan.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <cstdint>

#define MAX_FRAMES_IN_FLIGHT 2

// -------------------------------------------------------------------------
// QueueFamilyIndices
// Stores one queue family index per role. A single physical family can
// cover multiple roles; UINT32_MAX means "not found yet".
// -------------------------------------------------------------------------
struct QueueFamilyIndices
{
    uint32_t graphics = UINT32_MAX;
    uint32_t present  = UINT32_MAX;
    uint32_t transfer = UINT32_MAX;   // dedicated transfer; falls back to graphics
    uint32_t compute  = UINT32_MAX;   // dedicated compute;  falls back to graphics

    bool IsComplete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
};

// -------------------------------------------------------------------------
// VulkanDevice
// Owns every raw Vulkan object needed to create an nvrhi::vulkan::IDevice.
//
// Typical usage:
//   VulkanDevice dev(glfwWindow);
//   nvrhi::IDevice* device = dev.CreateDevice();
//   dev.CreateSwapchain(width, height);
//   // render loop …
//   dev.DestroyDevice();
//
// The GLFW window is used only for surface creation; VulkanDevice does
// not take ownership of it.
// -------------------------------------------------------------------------
class VulkanDevice : public IRenderDevice
{
public:
    explicit VulkanDevice(GLFWwindow* window);
    ~VulkanDevice() override;

    // IRenderDevice --------------------------------------------------------
    nvrhi::IDevice* CreateDevice()                                     override;
    void            DestroyDevice()                                    override;
    void            CreateSwapchain(uint32_t w, uint32_t h)            override;
    void            RecreateSwapchain(uint32_t width, uint32_t height) override;
	void			BeginFrame()                                       override;
    void            Present()                                          override;

    // Accessors — used by swapchain, frame managers, etc. ------------------
    VkInstance               GetInstance()       const { return m_Instance;       }
    VkPhysicalDevice         GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice                 GetLogicalDevice()  const { return m_Device;         }
    VkSurfaceKHR             GetSurface()        const { return m_Surface;        }
    const QueueFamilyIndices& GetQueueFamilies() const { return m_Queues;         }
    VkQueue GetGraphicsQueue()  const { return m_GraphicsQueue;  }
    VkQueue GetPresentQueue()   const { return m_PresentQueue;   }
    VkQueue GetTransferQueue()  const { return m_TransferQueue;  }
    VkQueue GetComputeQueue()   const { return m_ComputeQueue;   }
    VkSwapchainKHR              GetSwapchain()              const { return m_Swapchain;         }
    VkFormat                    GetSwapchainFormat()        const { return m_SwapFormat;        }
    VkExtent2D                  GetSwapchainExtent()        const { return m_SwapExtent;        }
    std::vector<VkImage>        GetSwapchainImages()        const { return m_SwapImages;        }
    uint32_t                    GetCurrentImageIndex()      const { return m_CurrentFrameIdx;   }
    const std::vector<nvrhi::TextureHandle>& GetBackBuffers() const { return m_BackBuffers;    }

private:
    // Init steps (called in sequence by CreateDevice) ----------------------
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    // Physical-device helpers ----------------------------------------------
    bool               IsDeviceSuitable(VkPhysicalDevice device)      const;
    bool               CheckExtensionSupport(VkPhysicalDevice device) const;
    int                ScoreDevice(VkPhysicalDevice device)           const;
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)     const;

    // Swapchain helpers ----------------------------------------------------
    VkSurfaceFormatKHR          ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR            ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D                  ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t width, uint32_t height);
    void                        CreateSyncObjects();
	nvrhi::Format			    ToNvrhiFormat(VkFormat format) const;
	void                        CreateHandleForNativeTextures();


    // NVRHI message sink — routes NVRHI diagnostics into spdlog -----------
    class MessageCallback : public nvrhi::IMessageCallback
    {
    public:
        void message(nvrhi::MessageSeverity severity, const char* text) override;
    };

    // Non-owning window pointer
    GLFWwindow* m_Window = nullptr;

    // Core Vulkan objects
    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_Surface        = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;

    // Queues
    QueueFamilyIndices m_Queues;
    VkQueue            m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue            m_PresentQueue  = VK_NULL_HANDLE;
    VkQueue            m_TransferQueue = VK_NULL_HANDLE;
    VkQueue            m_ComputeQueue  = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR              m_Swapchain     = VK_NULL_HANDLE;
    VkFormat                    m_SwapFormat    = VK_FORMAT_UNDEFINED;
    VkExtent2D                  m_SwapExtent    {};
    std::vector<VkImage>        m_SwapImages; // Owned by swapchain
	std::vector<nvrhi::TextureHandle> m_BackBuffers; // Wrappers around the above, owned by NVRHI

    // In-flight frame sync objects
    VkSemaphore m_ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
    // One render-finished semaphore per swapchain image (not per frame-in-flight),
    // because the presentation engine may still hold the semaphore when the
    // same frame slot cycles around.
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    // CPU fence per frame slot — submitted after all NVRHI work in Present(),
    // waited on in BeginFrame() before reusing the image-available semaphore.
    VkFence m_FrameFences[MAX_FRAMES_IN_FLIGHT] = {};

    // Present
	uint32_t m_CurrentFrame = 0; // 0 .. MAX_FRAMES_IN_FLIGHT-1
	uint32_t m_CurrentFrameIdx = 0; // index of the acquired swapchain image for this frame

    // NVRHI
    nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
    MessageCallback             m_MessageCallback;

    // Extension name arrays kept alive for the DeviceDesc lifetime
    std::vector<const char*> m_InstanceExtensions;
    std::vector<const char*> m_DeviceExtensions;
};

#endif // VULKAN_DEVICE_H
