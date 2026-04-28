#include "demo/dx12_wind_tunnel/presenter.h"

#include <algorithm>
#include <iostream>

namespace osr::demo::dx12_wind_tunnel {

namespace {

template <typename T>
void SafeRelease(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

bool Failed(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) {
        return false;
    }
    std::cerr << what << " failed, HRESULT=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* present = reinterpret_cast<PresentState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }
    case WM_CLOSE:
        if (present) {
            present->running = false;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (present) {
            present->running = false;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

} // namespace

bool CreatePresentState(HINSTANCE instance,
                        ID3D12Device* device,
                        ID3D12CommandQueue* queue,
                        core::Dimensions display_size,
                        PresentState& present) {
    WNDCLASSA wc {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "OSRDX12WindTunnel";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    RECT rect {0, 0, static_cast<LONG>(display_size.width), static_cast<LONG>(display_size.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    present.hwnd = CreateWindowExA(0,
                                   wc.lpszClassName,
                                   "oSR DX12 Wind Tunnel",
                                   WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   rect.right - rect.left,
                                   rect.bottom - rect.top,
                                   nullptr,
                                   nullptr,
                                   instance,
                                   &present);
    if (!present.hwnd) {
        return false;
    }

    if (Failed(CreateDXGIFactory1(IID_PPV_ARGS(&present.factory)), "CreateDXGIFactory1")) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc {};
    desc.Width = display_size.width;
    desc.Height = display_size.height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBackBufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGISwapChain1* swapchain1 = nullptr;
    if (Failed(present.factory->CreateSwapChainForHwnd(queue, present.hwnd, &desc, nullptr, nullptr, &swapchain1),
               "CreateSwapChainForHwnd")) {
        return false;
    }
    if (Failed(swapchain1->QueryInterface(IID_PPV_ARGS(&present.swapchain)), "Query swapchain3")) {
        SafeRelease(swapchain1);
        return false;
    }
    SafeRelease(swapchain1);

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = kBackBufferCount;
    if (Failed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&present.rtv_heap)), "Create RTV heap")) {
        return false;
    }
    present.rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = present.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        if (Failed(present.swapchain->GetBuffer(i, IID_PPV_ARGS(&present.back_buffers[i])), "Get swapchain buffer")) {
            return false;
        }
        device->CreateRenderTargetView(present.back_buffers[i], nullptr, handle);
        handle.ptr += present.rtv_descriptor_size;
    }
    return true;
}

void ReleasePresentState(PresentState& present) {
    for (auto*& buffer : present.back_buffers) {
        SafeRelease(buffer);
    }
    SafeRelease(present.rtv_heap);
    SafeRelease(present.swapchain);
    SafeRelease(present.factory);
}

bool PumpWindowMessages(PresentState& present) {
    MSG msg {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return present.running;
}

bool PresentOutputTexture(ID3D12CommandQueue* queue,
                          ID3D12CommandAllocator* allocator,
                          ID3D12GraphicsCommandList* command_list,
                          Dx12Sync& sync,
                          ID3D12Resource* color_output,
                          PresentState& present) {
    const UINT index = present.swapchain->GetCurrentBackBufferIndex();
    ID3D12Resource* backbuffer = present.back_buffers[index];
    if (Failed(allocator->Reset(), "Reset allocator for present") ||
        Failed(command_list->Reset(allocator, nullptr), "Reset command list for present")) {
        return false;
    }

    D3D12_RESOURCE_BARRIER barriers[2] {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = color_output;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = backbuffer;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    command_list->ResourceBarrier(2, barriers);
    command_list->CopyResource(backbuffer, color_output);
    std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
    std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
    command_list->ResourceBarrier(2, barriers);

    if (!ExecuteAndWait(queue, command_list, sync)) {
        return false;
    }
    present.swapchain->Present(1, 0);
    return true;
}

} // namespace osr::demo::dx12_wind_tunnel
