Texture2D<float4> g_input_color : register(t0);
RWTexture2D<float4> g_output_color : register(u0);

cbuffer UpscaleConstants : register(b0)
{
    uint2 g_input_size;
    uint2 g_output_size;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_output_size.x || dispatch_thread_id.y >= g_output_size.y)
    {
        return;
    }

    uint2 src = min((dispatch_thread_id.xy * g_input_size) / g_output_size, g_input_size - 1);
    g_output_color[dispatch_thread_id.xy] = g_input_color.Load(int3(src, 0));
}
