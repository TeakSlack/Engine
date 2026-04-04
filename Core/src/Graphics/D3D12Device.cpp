#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "D3D12Device.h"
#include "Util/Log.h"
#include "Render/NvrhiGpuDevice.h"

#include <nvrhi/d3d12.h>   // includes <directx/d3d12.h>
#include <dxgi1_6.h>       // IDXGIFactory6, IDXGIAdapter1, IDXGISwapChain3
#include <wrl/client.h>    // ComPtr

#include <vector>
#include <cstdio>

// =========================================================================
// Constants
// =========================================================================

static constexpr UINT SWAP_BUFFER_COUNT = 2;

#ifdef NDEBUG
static constexpr bool ENABLE_DEBUG_LAYER = false;
#else
static constexpr bool ENABLE_DEBUG_LAYER = true;
#endif

// =========================================================================
// HRESULT check helper
// =========================================================================

#define CHECK_HR(hr, msg) \
    if (FAILED(hr)) { CORE_ERROR("[D3D12Device] {}: HRESULT 0x{:08X}", (msg), (uint32_t)(hr)); abort(); }

// =========================================================================
// MessageCallback — routes NVRHI diagnostics to CORE_* macros
// =========================================================================

namespace {

class MessageCallback : public nvrhi::IMessageCallback
{
public:
    void message(nvrhi::MessageSeverity severity, const char* text) override;
};

void MessageCallback::message(nvrhi::MessageSeverity severity, const char* text)
{
    switch (severity)
    {
    case nvrhi::MessageSeverity::Fatal:
        CORE_ERROR(text);
        abort();
        break;
    case nvrhi::MessageSeverity::Error:
        CORE_ERROR(text);
        break;
    case nvrhi::MessageSeverity::Warning:
        CORE_WARN(text);
        break;
    case nvrhi::MessageSeverity::Info:
        CORE_INFO(text);
        break;
    }
}

} // namespace

// =========================================================================
// D3D12Device::Impl — all D3D12, DXGI, and NVRHI-D3D12 state lives here
// =========================================================================

struct D3D12Device::Impl
{
    // Non-owning window handle
    HWND m_Hwnd = nullptr;

    // DXGI / D3D12 core objects
    Microsoft::WRL::ComPtr<IDXGIFactory6>      m_Factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1>      m_Adapter;
    Microsoft::WRL::ComPtr<ID3D12Device>       m_Device;

    // Command queues
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_ComputeQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CopyQueue;

    // Swapchain
    Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_Swapchain;
    DXGI_FORMAT                                         m_SwapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t                                            m_SwapWidth  = 0;
    uint32_t                                            m_SwapHeight = 0;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_SwapResources;
    std::vector<nvrhi::TextureHandle>                   m_BackBuffers;

    // Frame synchronisation
    Microsoft::WRL::ComPtr<ID3D12Fence> m_FrameFence;
    UINT64                              m_FenceValues[SWAP_BUFFER_COUNT] = {}; // per-buffer fence values
    UINT64                              m_NextFenceValue                 = 1;
    HANDLE                              m_FenceEvent                     = nullptr;
    uint32_t                            m_CurrentFrameIdx                = 0;

    // NVRHI
    nvrhi::d3d12::DeviceHandle      m_NvrhiDevice;
    MessageCallback                 m_MessageCallback;
    std::unique_ptr<NvrhiGpuDevice> m_GpuDevice;

    // ------------------------------------------------------------------
    // Init steps (called in sequence by D3D12Device::CreateDevice)
    // ------------------------------------------------------------------
    void EnableDebugLayer();
    void CreateFactory();
    void PickAdapter();
    void InitDevice();
    void CreateQueues();
    void CreateFence();
    void WrapBackBuffers();
    void WaitForAllFences();
};

// =========================================================================
// Impl — EnableDebugLayer
// =========================================================================

void D3D12Device::Impl::EnableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (SUCCEEDED(hr))
    {
        debugController->EnableDebugLayer();
        CORE_INFO("[D3D12Device] D3D12 debug layer enabled");
    }
    else
    {
        CORE_WARN("[D3D12Device] D3D12 debug layer not available (HRESULT 0x{:08X})", (uint32_t)hr);
    }
}

// =========================================================================
// Impl — CreateFactory
// =========================================================================

void D3D12Device::Impl::CreateFactory()
{
    UINT flags = ENABLE_DEBUG_LAYER ? DXGI_CREATE_FACTORY_DEBUG : 0;
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_Factory));
    CHECK_HR(hr, "CreateDXGIFactory2 failed");
}

// =========================================================================
// Impl — PickAdapter
// =========================================================================

void D3D12Device::Impl::PickAdapter()
{
    for (UINT i = 0; ; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        HRESULT hr = m_Factory->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));

        if (hr == DXGI_ERROR_NOT_FOUND)
            break;

        CHECK_HR(hr, "EnumAdapterByGpuPreference failed");

        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software (WARP) adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Test whether a D3D12 device can be created without actually creating one
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                               __uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {
            // EnumAdapterByGpuPreference already orders by preference, so
            // the first suitable adapter is the best one.
            m_Adapter = adapter;

            char name[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                                name, sizeof(name), nullptr, nullptr);
            CORE_INFO("[D3D12Device] Adapter selected: {} ({} MB VRAM)",
                      name, desc.DedicatedVideoMemory / (1024 * 1024));
            return;
        }
    }

    CORE_ERROR("[D3D12Device] No suitable D3D12 adapter found (feature level 12.0)");
    abort();
}

// =========================================================================
// Impl — InitDevice
// =========================================================================

void D3D12Device::Impl::InitDevice()
{
    HRESULT hr = D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                   IID_PPV_ARGS(&m_Device));
    CHECK_HR(hr, "D3D12CreateDevice failed");

    if (ENABLE_DEBUG_LAYER)
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_Device.As(&infoQueue)))
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,      TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        }
    }

    CORE_INFO("[D3D12Device] D3D12 device created (feature level 12.0)");
}

// =========================================================================
// Impl — CreateQueues
// =========================================================================

void D3D12Device::Impl::CreateQueues()
{
    // Graphics queue
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue));
        CHECK_HR(hr, "CreateCommandQueue (graphics) failed");

        if (ENABLE_DEBUG_LAYER)
            m_GraphicsQueue->SetName(L"GraphicsQueue");
    }

    // Compute queue
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
        CHECK_HR(hr, "CreateCommandQueue (compute) failed");

        if (ENABLE_DEBUG_LAYER)
            m_ComputeQueue->SetName(L"ComputeQueue");
    }

    // Copy queue
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
        CHECK_HR(hr, "CreateCommandQueue (copy) failed");

        if (ENABLE_DEBUG_LAYER)
            m_CopyQueue->SetName(L"CopyQueue");
    }
}

// =========================================================================
// Impl — CreateFence
// =========================================================================

void D3D12Device::Impl::CreateFence()
{
    HRESULT hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
    CHECK_HR(hr, "CreateFence failed");

    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_FenceEvent)
    {
        CORE_ERROR("[D3D12Device] CreateEvent for fence failed");
        abort();
    }

    for (UINT f = 0; f < SWAP_BUFFER_COUNT; ++f)
        m_FenceValues[f] = 0;
}

// =========================================================================
// Impl — WrapBackBuffers
// =========================================================================

void D3D12Device::Impl::WrapBackBuffers()
{
    m_SwapResources.resize(SWAP_BUFFER_COUNT);
    m_BackBuffers.resize(SWAP_BUFFER_COUNT);

    for (UINT i = 0; i < SWAP_BUFFER_COUNT; ++i)
    {
        HRESULT hr = m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&m_SwapResources[i]));
        CHECK_HR(hr, "IDXGISwapChain3::GetBuffer failed");

        char debugName[32];
        std::snprintf(debugName, sizeof(debugName), "BackBuffer[%u]", i);

        nvrhi::TextureDesc texDesc;
        texDesc.width            = m_SwapWidth;
        texDesc.height           = m_SwapHeight;
        texDesc.format           = nvrhi::Format::RGBA8_UNORM;
        texDesc.dimension        = nvrhi::TextureDimension::Texture2D;
        texDesc.isRenderTarget   = true;
        texDesc.debugName        = debugName;
        texDesc.initialState     = nvrhi::ResourceStates::Present;
        texDesc.keepInitialState = true;

        m_BackBuffers[i] = m_NvrhiDevice->createHandleForNativeTexture(
            nvrhi::ObjectTypes::D3D12_Resource,
            nvrhi::Object(m_SwapResources[i].Get()),
            texDesc);
    }
    m_GpuDevice->RegisterBackBuffers(m_BackBuffers);
}

// =========================================================================
// Impl — WaitForAllFences
// =========================================================================
void D3D12Device::Impl::WaitForAllFences()
{
    for (UINT i = 0; i < SWAP_BUFFER_COUNT; ++i)
    {
        if (m_FrameFence->GetCompletedValue() < m_FenceValues[i])
        {
            m_FrameFence->SetEventOnCompletion(m_FenceValues[i], m_FenceEvent);
            WaitForSingleObject(m_FenceEvent, INFINITE);
        }
    }
}

// =========================================================================
// D3D12Device — lifecycle
// =========================================================================

D3D12Device::D3D12Device(void* hwnd)
    : m_Impl(std::make_unique<Impl>())
{
    m_Impl->m_Hwnd = static_cast<HWND>(hwnd);
}

D3D12Device::~D3D12Device()
{
    if (m_Impl->m_NvrhiDevice)
        DestroyDevice();
}

IGpuDevice* D3D12Device::CreateDevice()
{
    auto& i = *m_Impl;

    if (ENABLE_DEBUG_LAYER)
        i.EnableDebugLayer();

    i.CreateFactory();
    i.PickAdapter();
    i.InitDevice();
    i.CreateQueues();
    i.CreateFence();

    nvrhi::d3d12::DeviceDesc desc;
    desc.errorCB                 = &i.m_MessageCallback;
    desc.pDevice                 = i.m_Device.Get();
    desc.pGraphicsCommandQueue   = i.m_GraphicsQueue.Get();
    desc.pComputeCommandQueue    = i.m_ComputeQueue.Get();
    desc.pCopyCommandQueue       = i.m_CopyQueue.Get();

    i.m_NvrhiDevice = nvrhi::d3d12::createDevice(desc);
    if (!i.m_NvrhiDevice)
    {
        CORE_ERROR("[D3D12Device] nvrhi::d3d12::createDevice() returned null");
        abort();
    }

    CORE_INFO("[D3D12Device] NVRHI D3D12 device created");
    i.m_GpuDevice = std::make_unique<NvrhiGpuDevice>(i.m_NvrhiDevice.Get());
    return i.m_GpuDevice.get();
}

void D3D12Device::DestroyDevice()
{
    auto& i = *m_Impl;

    if (i.m_NvrhiDevice)
    {
        i.m_NvrhiDevice->waitForIdle();
        // Release GpuDevice (holds non-owning ptr to NVRHI device) first,
        // then clear NVRHI texture wrappers, then destroy the device.
        i.m_GpuDevice.reset();
        i.m_BackBuffers.clear();
        i.m_SwapResources.clear();
        i.m_NvrhiDevice = nullptr;
    }

    if (i.m_Swapchain)
        i.m_Swapchain.Reset();

    if (i.m_FenceEvent)
    {
        CloseHandle(i.m_FenceEvent);
        i.m_FenceEvent = nullptr;
    }

    i.m_FrameFence.Reset();
    i.m_CopyQueue.Reset();
    i.m_ComputeQueue.Reset();
    i.m_GraphicsQueue.Reset();
    i.m_Device.Reset();
    i.m_Adapter.Reset();
    i.m_Factory.Reset();
}

// =========================================================================
// D3D12Device — swapchain
// =========================================================================

void D3D12Device::CreateSwapchain(uint32_t w, uint32_t h)
{
    auto& i = *m_Impl;

    i.m_SwapWidth  = w;
    i.m_SwapHeight = h;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width       = w;
    desc.Height      = h;
    desc.Format      = i.m_SwapFormat;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = SWAP_BUFFER_COUNT;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags       = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = i.m_Factory->CreateSwapChainForHwnd(
        i.m_GraphicsQueue.Get(), i.m_Hwnd, &desc, nullptr, nullptr, &sc1);
    CHECK_HR(hr, "CreateSwapChainForHwnd failed");

    hr = sc1.As(&i.m_Swapchain);
    CHECK_HR(hr, "QueryInterface IDXGISwapChain3 failed");

    // Disable fullscreen toggle on Alt+Enter
    i.m_Factory->MakeWindowAssociation(i.m_Hwnd, DXGI_MWA_NO_ALT_ENTER);

    i.WrapBackBuffers();

    CORE_INFO("[D3D12Device] Swapchain created ({}x{}, DXGI_FORMAT {})",
              w, h, (int)i.m_SwapFormat);
}

void D3D12Device::RecreateSwapchain(uint32_t width, uint32_t height)
{
    auto& i = *m_Impl;

    i.m_NvrhiDevice->waitForIdle();
    i.WaitForAllFences();

    // Release NVRHI wrappers and underlying resources before resize
    i.m_GpuDevice->ClearBackBuffers();
    i.m_BackBuffers.clear();
    i.m_SwapResources.clear();

    DXGI_SWAP_CHAIN_DESC desc = {};
    i.m_Swapchain->GetDesc(&desc);

    HRESULT hr = i.m_Swapchain->ResizeBuffers(
        desc.BufferCount, width, height, desc.BufferDesc.Format, 0);
    CHECK_HR(hr, "IDXGISwapChain3::ResizeBuffers failed");

    i.m_SwapWidth  = width;
    i.m_SwapHeight = height;

    i.WrapBackBuffers();

    CORE_INFO("[D3D12Device] Swapchain resized to {}x{}", width, height);
}

// =========================================================================
// D3D12Device — per-frame
// =========================================================================

void D3D12Device::BeginFrame()
{
    auto& i = *m_Impl;

    i.m_CurrentFrameIdx = i.m_Swapchain->GetCurrentBackBufferIndex();

    // Wait until the GPU has finished using this back buffer slot
    if (i.m_FrameFence->GetCompletedValue() < i.m_FenceValues[i.m_CurrentFrameIdx])
    {
        i.m_FrameFence->SetEventOnCompletion(
            i.m_FenceValues[i.m_CurrentFrameIdx], i.m_FenceEvent);
        WaitForSingleObject(i.m_FenceEvent, INFINITE);
    }
}

void D3D12Device::Present()
{
    auto& i = *m_Impl;

    HRESULT hr = i.m_Swapchain->Present(1, 0); // vsync on
    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        HRESULT reason = i.m_Device->GetDeviceRemovedReason();
        CORE_ERROR("[D3D12Device] Device removed during Present (reason: 0x{:08X})", (uint32_t)reason);
        abort();
    }
    else if (FAILED(hr))
    {
        CORE_ERROR("[D3D12Device] IDXGISwapChain3::Present failed: HRESULT 0x{:08X}", (uint32_t)hr);
        abort();
    }

    // Record the fence value that will be signalled after this frame's work
    i.m_FenceValues[i.m_CurrentFrameIdx] = i.m_NextFenceValue;
    i.m_GraphicsQueue->Signal(i.m_FrameFence.Get(), i.m_NextFenceValue++);
}

// =========================================================================
// D3D12Device — accessors
// =========================================================================

uint32_t D3D12Device::GetCurrentImageIndex() const
{
    return m_Impl->m_CurrentFrameIdx;
}
