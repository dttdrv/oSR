# Source Index

Primary sources used by this wiki. Add new sources here first, then cite them from topic pages.

## Vendor And API Sources

- AMD FSR SDK landing page: <https://gpuopen.com/amd-fsr-sdk/>
- AMD FSR3 upscaler manual: <https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/>
- AMD FSR temporal manual: <https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-temporal/>
- AMD FSR ML/upscaling manual: <https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-ml/>
- Local AMD SDK checkout, not vendored: `external/FidelityFX-SDK`, observed commit in `STATE.yaml`.
- Intel XeSS-SR developer guide: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- Intel XeSS SDK repository: <https://github.com/intel/xess>
- Intel XeSS Inspector article: <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-xess-inspector.html>
- Intel VALAR + XeSS article: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-velocity-and-luminance-adaptive-rasterization.html>
- NVIDIA Streamline Programming Guide: <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md>
- NVIDIA DLSS 4 AI innovations article: <https://www.nvidia.com/en-us/geforce/news/gfecnt/20251/dlss4-multi-frame-generation-ai-innovations/>
- NVIDIA DLSS 4.5 developer article: <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>
- Microsoft DirectSR spec: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>
- OptiScaler repository, clean-room reference only: <https://github.com/optiscaler/OptiScaler>

## Research Sources

- Yang et al., survey of temporal antialiasing: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>
- Schied et al., Spatiotemporal Variance-Guided Filtering: <https://research.nvidia.com/labs/rtr/publication/schied2017spatiotemporal/>
- Bitterli et al., ReSTIR: <https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/>
- Zhang et al., Area ReSTIR: <https://research.nvidia.com/labs/rtr/publication/zhang2024area/>
- Unreal Engine TSR documentation: <https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine>
- SwinIR paper: <https://arxiv.org/abs/2108.10257>

## Local Code Evidence

- Internal normalized input contract: `src/core/frame_context.h`.
- Frame-context validation: `src/core/frame_context.cpp`.
- FSR-style normalization bridge: `src/interop/fsr2_bridge/fsr_bridge.cpp`.
- XeSS diagnostic proxy entrypoints: `src/interop/xess_bridge/xess_proxy.cpp`.
- CPU trust-field oracle: `src/reconstruction/trust_field.*`, `src/reconstruction/temporal_oracle.*`.
- Tile risk routing: `src/reconstruction/tile_classifier.*`.
- Bounded residual search prototype: `src/reconstruction/residual_search.*`.
- Feature locks: `src/reconstruction/feature_locks.*`.
- DX12 temporal shader path: `src/reconstruction/shaders/temporal_resolve.hlsl`, `src/backends/dx12/temporal_resolve_pass.cpp`.
- Capture analysis and gates: `src/debug/capture_analysis.*`, `src/debug/capture_compare.*`, `profiles/capture_gate.cfg`.

## Licensing Boundaries

- AMD FidelityFX headers/source in the SDK include permissive notices in many files, but signed SDK binary distribution terms must still be respected.
- Intel XeSS SDK license permits use of the redistributable runtime but prohibits reverse engineering/modification of binaries.
- NVIDIA DLSS and Streamline are integration surfaces; DLSS model internals are not public source.
- OptiScaler is GPL and useful as an architecture signal only unless oSR intentionally becomes GPL-compatible.
