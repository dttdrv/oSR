#pragma once

#include "core/frame_context.h"
#include "demo/dx12_wind_tunnel/dx12_texture_io.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>

namespace osr::demo::dx12_wind_tunnel {

constexpr uint32_t kBackBufferCount = 2;

struct PresentState {
    HWND hwnd = nullptr;
    IDXGIFactory4* factory = nullptr;
    IDXGISwapChain3* swapchain = nullptr;
    ID3D12DescriptorHeap* rtv_heap = nullptr;
    ID3D12Resource* back_buffers[kBackBufferCount] {};
    UINT rtv_descriptor_size = 0;
    bool running = true;
};

bool CreatePresentState(HINSTANCE instance,
                        ID3D12Device* device,
                        ID3D12CommandQueue* queue,
                        core::Dimensions display_size,
                        PresentState& present);
void ReleasePresentState(PresentState& present);
bool PumpWindowMessages(PresentState& present);
bool PresentOutputTexture(ID3D12CommandQueue* queue,
                          ID3D12CommandAllocator* allocator,
                          ID3D12GraphicsCommandList* command_list,
                          Dx12Sync& sync,
                          ID3D12Resource* color_output,
                          PresentState& present);

} // namespace osr::demo::dx12_wind_tunnel
