Texture2D<float> g_color_delta_luma : register(t0);
Texture2D<float> g_depth_delta : register(t1);
Texture2D<float2> g_motion_vectors : register(t2);
Texture2D<float> g_transparency_hint : register(t3);
RWTexture2D<float> g_synthesized_reactive : register(u0);

cbuffer ReactiveSynthesisConstants : register(b0)
{
    uint2 g_output_size;
    float g_color_delta_scale;
    float g_depth_delta_scale;
    float g_motion_scale;
    float g_transparency_scale;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_output_size.x || dispatch_thread_id.y >= g_output_size.y)
    {
        return;
    }

    uint2 px = dispatch_thread_id.xy;
    float color_term = abs(g_color_delta_luma[px]) * g_color_delta_scale;
    float depth_term = abs(g_depth_delta[px]) * g_depth_delta_scale;
    float motion_term = length(g_motion_vectors[px]) * g_motion_scale;
    float transparency_term = g_transparency_hint[px] * g_transparency_scale;
    g_synthesized_reactive[px] = saturate(max(max(color_term, depth_term), max(motion_term, transparency_term)));
}

