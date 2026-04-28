#pragma once

#include "core/frame_context.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace osr::demo::dx12_wind_tunnel {

struct Dx12Sync {
    ID3D12Fence* fence = nullptr;
    HANDLE fence_event = nullptr;
    uint64_t fence_value = 0;
};

struct TextureTransferResult {
    std::string name;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    core::Dimensions extent {};
    uint64_t row_size_bytes = 0;
    uint32_t row_pitch = 0;
    uint64_t total_bytes = 0;
    uint64_t cpu_hash = 0;
    uint64_t gpu_hash = 0;
    bool matched = false;
};

bool InitializeSync(ID3D12Device* device, Dx12Sync& sync);
void ReleaseSync(Dx12Sync& sync);
bool ExecuteAndWait(ID3D12CommandQueue* queue, ID3D12GraphicsCommandList* command_list, Dx12Sync& sync);

bool UploadReadbackTexture2D(ID3D12Device* device,
                             ID3D12CommandQueue* queue,
                             ID3D12CommandAllocator* allocator,
                             ID3D12GraphicsCommandList* command_list,
                             Dx12Sync& sync,
                             ID3D12Resource* texture,
                             DXGI_FORMAT format,
                             core::Dimensions extent,
                             const void* source,
                             uint64_t source_row_bytes,
                             const std::string& name,
                             TextureTransferResult& result);

bool ReadbackTexture2D(ID3D12Device* device,
                       ID3D12CommandQueue* queue,
                       ID3D12CommandAllocator* allocator,
                       ID3D12GraphicsCommandList* command_list,
                       Dx12Sync& sync,
                       ID3D12Resource* texture,
                       DXGI_FORMAT format,
                       core::Dimensions extent,
                       const void* expected,
                       uint64_t expected_row_bytes,
                       const std::string& name,
                       TextureTransferResult& result);

[[nodiscard]] bool AllTransfersMatched(const std::vector<TextureTransferResult>& results) noexcept;

} // namespace osr::demo::dx12_wind_tunnel
