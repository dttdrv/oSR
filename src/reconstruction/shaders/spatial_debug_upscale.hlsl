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

    float2 src = ((float2(dispatch_thread_id.xy) + 0.5f) * float2(g_input_size) / float2(g_output_size)) - 0.5f;
    src = clamp(src, 0.0f, float2(g_input_size - 1));

    uint2 p0 = uint2(floor(src));
    uint2 p1 = min(p0 + 1, g_input_size - 1);
    float2 t = src - float2(p0);

    float4 c00 = g_input_color.Load(int3(p0, 0));
    float4 c10 = g_input_color.Load(int3(p1.x, p0.y, 0));
    float4 c01 = g_input_color.Load(int3(p0.x, p1.y, 0));
    float4 c11 = g_input_color.Load(int3(p1, 0));
    float4 top = lerp(c00, c10, t.x);
    float4 bottom = lerp(c01, c11, t.x);
    float4 color = lerp(top, bottom, t.y);
    g_output_color[dispatch_thread_id.xy] = floor(saturate(color) * 255.0f + 0.5f) / 255.0f;
}
