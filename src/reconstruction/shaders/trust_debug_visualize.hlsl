Texture2D<float> g_history_trust : register(t0);
Texture2D<float> g_accumulation_weight : register(t1);
RWTexture2D<float4> g_debug_output : register(u0);

cbuffer TrustDebugConstants : register(b0)
{
    uint2 g_output_size;
    uint g_mode;
    uint g_padding;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_output_size.x || dispatch_thread_id.y >= g_output_size.y)
    {
        return;
    }

    uint2 px = dispatch_thread_id.xy;
    float value = g_mode == 0 ? g_history_trust[px] : g_accumulation_weight[px];
    g_debug_output[px] = float4(1.0f - value, value, 0.1f, 1.0f);
}

