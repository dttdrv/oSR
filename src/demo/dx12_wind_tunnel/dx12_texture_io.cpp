#include "demo/dx12_wind_tunnel/dx12_texture_io.h"

#include "debug/capture_pack.h"

#include <algorithm>
#include <cstring>
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

bool CreateBuffer(ID3D12Device* device,
                  D3D12_HEAP_TYPE heap_type,
                  uint64_t size,
                  D3D12_RESOURCE_STATES initial_state,
                  ID3D12Resource** out_resource) {
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = heap_type;

    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    return SUCCEEDED(device->CreateCommittedResource(&heap,
                                                     D3D12_HEAP_FLAG_NONE,
                                                     &desc,
                                                     initial_state,
                                                     nullptr,
                                                     IID_PPV_ARGS(out_resource)));
}

uint64_t HashLinearRows(const uint8_t* data, uint64_t row_pitch, uint64_t row_size, uint32_t rows) {
    constexpr uint64_t kOffset = 14695981039346656037ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = kOffset;
    for (uint32_t y = 0; y < rows; ++y) {
        const uint8_t* row = data + static_cast<uint64_t>(y) * row_pitch;
        for (uint64_t x = 0; x < row_size; ++x) {
            hash ^= row[x];
            hash *= kPrime;
        }
    }
    return hash;
}

} // namespace

bool InitializeSync(ID3D12Device* device, Dx12Sync& sync) {
    if (Failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&sync.fence)), "CreateFence")) {
        return false;
    }
    sync.fence_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!sync.fence_event) {
        return false;
    }
    sync.fence_value = 0;
    return true;
}

void ReleaseSync(Dx12Sync& sync) {
    if (sync.fence_event) {
        CloseHandle(sync.fence_event);
        sync.fence_event = nullptr;
    }
    SafeRelease(sync.fence);
    sync.fence_value = 0;
}

bool ExecuteAndWait(ID3D12CommandQueue* queue, ID3D12GraphicsCommandList* command_list, Dx12Sync& sync) {
    if (Failed(command_list->Close(), "Close command list")) {
        return false;
    }
    ID3D12CommandList* lists[] = {command_list};
    queue->ExecuteCommandLists(1, lists);
    const uint64_t fence_value = ++sync.fence_value;
    if (Failed(queue->Signal(sync.fence, fence_value), "Queue signal")) {
        return false;
    }
    if (sync.fence->GetCompletedValue() < fence_value) {
        if (Failed(sync.fence->SetEventOnCompletion(fence_value, sync.fence_event), "SetEventOnCompletion")) {
            return false;
        }
        WaitForSingleObject(sync.fence_event, INFINITE);
    }
    return true;
}

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
                             TextureTransferResult& result) {
    if (!device || !queue || !allocator || !command_list || !texture || !source) {
        return false;
    }

    const auto desc = texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
    UINT rows = 0;
    UINT64 row_size = 0;
    UINT64 total_bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &row_size, &total_bytes);

    ID3D12Resource* upload = nullptr;
    ID3D12Resource* readback = nullptr;
    if (!CreateBuffer(device, D3D12_HEAP_TYPE_UPLOAD, total_bytes, D3D12_RESOURCE_STATE_GENERIC_READ, &upload) ||
        !CreateBuffer(device, D3D12_HEAP_TYPE_READBACK, total_bytes, D3D12_RESOURCE_STATE_COPY_DEST, &readback)) {
        SafeRelease(upload);
        SafeRelease(readback);
        return false;
    }

    uint8_t* mapped_upload = nullptr;
    D3D12_RANGE empty_range {0, 0};
    if (Failed(upload->Map(0, &empty_range, reinterpret_cast<void**>(&mapped_upload)), "Map upload")) {
        SafeRelease(upload);
        SafeRelease(readback);
        return false;
    }
    const auto* src = static_cast<const uint8_t*>(source);
    for (UINT y = 0; y < rows; ++y) {
        std::memcpy(mapped_upload + footprint.Offset + static_cast<uint64_t>(y) * footprint.Footprint.RowPitch,
                    src + static_cast<uint64_t>(y) * source_row_bytes,
                    static_cast<size_t>(row_size));
    }
    upload->Unmap(0, nullptr);

    if (Failed(allocator->Reset(), "Reset allocator") ||
        Failed(command_list->Reset(allocator, nullptr), "Reset command list")) {
        SafeRelease(upload);
        SafeRelease(readback);
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION upload_location {};
    upload_location.pResource = upload;
    upload_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    upload_location.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION texture_location {};
    texture_location.pResource = texture;
    texture_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    texture_location.SubresourceIndex = 0;

    command_list->CopyTextureRegion(&texture_location, 0, 0, 0, &upload_location, nullptr);

    D3D12_RESOURCE_BARRIER to_copy_source {};
    to_copy_source.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to_copy_source.Transition.pResource = texture;
    to_copy_source.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    to_copy_source.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    to_copy_source.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    command_list->ResourceBarrier(1, &to_copy_source);

    D3D12_TEXTURE_COPY_LOCATION readback_location {};
    readback_location.pResource = readback;
    readback_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    readback_location.PlacedFootprint = footprint;
    command_list->CopyTextureRegion(&readback_location, 0, 0, 0, &texture_location, nullptr);

    D3D12_RESOURCE_BARRIER to_shader_read = to_copy_source;
    to_shader_read.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    to_shader_read.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    command_list->ResourceBarrier(1, &to_shader_read);

    if (!ExecuteAndWait(queue, command_list, sync)) {
        SafeRelease(upload);
        SafeRelease(readback);
        return false;
    }

    uint8_t* mapped_readback = nullptr;
    D3D12_RANGE read_range {static_cast<SIZE_T>(footprint.Offset), static_cast<SIZE_T>(footprint.Offset + total_bytes)};
    if (Failed(readback->Map(0, &read_range, reinterpret_cast<void**>(&mapped_readback)), "Map readback")) {
        SafeRelease(upload);
        SafeRelease(readback);
        return false;
    }

    result.name = name;
    result.format = format;
    result.extent = extent;
    result.row_size_bytes = row_size;
    result.row_pitch = footprint.Footprint.RowPitch;
    result.total_bytes = total_bytes;
    result.cpu_hash = HashLinearRows(src, source_row_bytes, row_size, rows);
    result.gpu_hash = HashLinearRows(mapped_readback + footprint.Offset, footprint.Footprint.RowPitch, row_size, rows);
    result.matched = result.cpu_hash == result.gpu_hash;

    D3D12_RANGE no_write {0, 0};
    readback->Unmap(0, &no_write);
    SafeRelease(upload);
    SafeRelease(readback);
    return true;
}

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
                       TextureTransferResult& result) {
    if (!device || !queue || !allocator || !command_list || !texture || !expected) {
        return false;
    }

    const auto desc = texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
    UINT rows = 0;
    UINT64 row_size = 0;
    UINT64 total_bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &row_size, &total_bytes);

    ID3D12Resource* readback = nullptr;
    if (!CreateBuffer(device, D3D12_HEAP_TYPE_READBACK, total_bytes, D3D12_RESOURCE_STATE_COPY_DEST, &readback)) {
        return false;
    }

    if (Failed(allocator->Reset(), "Reset allocator") ||
        Failed(command_list->Reset(allocator, nullptr), "Reset command list")) {
        SafeRelease(readback);
        return false;
    }

    D3D12_RESOURCE_BARRIER to_copy_source {};
    to_copy_source.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to_copy_source.Transition.pResource = texture;
    to_copy_source.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    to_copy_source.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    to_copy_source.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    command_list->ResourceBarrier(1, &to_copy_source);

    D3D12_TEXTURE_COPY_LOCATION texture_location {};
    texture_location.pResource = texture;
    texture_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    texture_location.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION readback_location {};
    readback_location.pResource = readback;
    readback_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    readback_location.PlacedFootprint = footprint;
    command_list->CopyTextureRegion(&readback_location, 0, 0, 0, &texture_location, nullptr);

    D3D12_RESOURCE_BARRIER to_shader_read = to_copy_source;
    to_shader_read.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    to_shader_read.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    command_list->ResourceBarrier(1, &to_shader_read);

    if (!ExecuteAndWait(queue, command_list, sync)) {
        SafeRelease(readback);
        return false;
    }

    uint8_t* mapped_readback = nullptr;
    D3D12_RANGE read_range {static_cast<SIZE_T>(footprint.Offset), static_cast<SIZE_T>(footprint.Offset + total_bytes)};
    if (Failed(readback->Map(0, &read_range, reinterpret_cast<void**>(&mapped_readback)), "Map readback")) {
        SafeRelease(readback);
        return false;
    }

    const auto* src = static_cast<const uint8_t*>(expected);
    result.name = name;
    result.format = format;
    result.extent = extent;
    result.row_size_bytes = row_size;
    result.row_pitch = footprint.Footprint.RowPitch;
    result.total_bytes = total_bytes;
    result.cpu_hash = HashLinearRows(src, expected_row_bytes, row_size, rows);
    result.gpu_hash = HashLinearRows(mapped_readback + footprint.Offset, footprint.Footprint.RowPitch, row_size, rows);
    result.matched = result.cpu_hash == result.gpu_hash;

    D3D12_RANGE no_write {0, 0};
    readback->Unmap(0, &no_write);
    SafeRelease(readback);
    return true;
}

bool AllTransfersMatched(const std::vector<TextureTransferResult>& results) noexcept {
    return std::all_of(results.begin(), results.end(), [](const TextureTransferResult& result) {
        return result.matched;
    });
}

} // namespace osr::demo::dx12_wind_tunnel
