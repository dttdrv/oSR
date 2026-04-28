Texture2D<float4> g_current_color : register(t0);
Texture2D<float4> g_previous_history : register(t1);
Texture2D<float> g_current_depth : register(t2);
Texture2D<float> g_previous_depth : register(t3);
Texture2D<float2> g_motion_vectors : register(t4);
Texture2D<float> g_reactive_mask : register(t5);
RWTexture2D<float4> g_output_color : register(u0);

cbuffer TemporalConstants : register(b0)
{
    uint2 g_render_size;
    uint2 g_display_size;
    float g_max_history_weight;
    float g_reactive_penalty;
    float g_motion_rejection_pixels;
    float g_color_rejection_threshold;
    float g_depth_rejection_threshold;
    float g_sharpening_amount;
    float g_sharpening_low_trust_scale;
    float g_sharpening_reactive_scale;
};

float Luma(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 QuantizeRgba8(float4 c)
{
    return floor(saturate(c) * 255.0f + 0.5f) / 255.0f;
}

float4 SampleRenderColor(Texture2D<float4> texture_source, float2 p);

float4 SampleCurrentDisplay(float2 display_px)
{
    float2 render_float = ((display_px + 0.5f) * float2(g_render_size) / float2(g_display_size)) - 0.5f;
    return QuantizeRgba8(SampleRenderColor(g_current_color, render_float));
}

float4 ApplyDetailRecovery(float4 resolved, float2 display_px, float history_weight, float reactive, bool disoccluded)
{
    if (g_sharpening_amount <= 0.0f || disoccluded)
    {
        return resolved;
    }

    float trust_scale = lerp(saturate(g_sharpening_low_trust_scale), 1.0f, saturate(history_weight));
    float reactive_scale = lerp(1.0f, saturate(g_sharpening_reactive_scale), saturate(reactive));
    float amount = saturate(g_sharpening_amount) * trust_scale * reactive_scale;
    if (amount <= 0.0f)
    {
        return resolved;
    }

    float2 left_px = float2(max(display_px.x - 1.0f, 0.0f), display_px.y);
    float2 right_px = float2(min(display_px.x + 1.0f, float(g_display_size.x - 1)), display_px.y);
    float2 up_px = float2(display_px.x, max(display_px.y - 1.0f, 0.0f));
    float2 down_px = float2(display_px.x, min(display_px.y + 1.0f, float(g_display_size.y - 1)));
    float3 center = SampleCurrentDisplay(display_px).rgb;
    float3 average = (SampleCurrentDisplay(left_px).rgb +
                      SampleCurrentDisplay(right_px).rgb +
                      SampleCurrentDisplay(up_px).rgb +
                      SampleCurrentDisplay(down_px).rgb) * 0.25f;
    resolved.rgb = saturate(resolved.rgb + (center - average) * amount);
    return resolved;
}

float4 SampleDisplay(Texture2D<float4> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_display_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_display_size - 1);
    float2 t = p - float2(p0);
    float4 c00 = texture_source.Load(int3(p0, 0));
    float4 c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float4 c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float4 c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

float4 SampleRenderColor(Texture2D<float4> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_render_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_render_size - 1);
    float2 t = p - float2(p0);
    float4 c00 = texture_source.Load(int3(p0, 0));
    float4 c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float4 c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float4 c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

float SampleRenderFloat(Texture2D<float> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_render_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_render_size - 1);
    float2 t = p - float2(p0);
    float c00 = texture_source.Load(int3(p0, 0));
    float c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_display_size.x || dispatch_thread_id.y >= g_display_size.y)
    {
        return;
    }

    uint2 out_px = dispatch_thread_id.xy;
    float2 display_px = float2(out_px);
    uint2 render_px = min((out_px * g_render_size) / g_display_size, g_render_size - 1);

    float4 current_color = SampleCurrentDisplay(display_px);
    float2 mv = g_motion_vectors.Load(int3(render_px, 0));
    float motion_len = length(mv);
    float history_weight = saturate(g_max_history_weight);

    float reactive = g_reactive_mask.Load(int3(render_px, 0));
    history_weight *= saturate(1.0f - reactive * g_reactive_penalty);

    float2 history_px = display_px;
    float previous_depth = g_previous_depth.Load(int3(render_px, 0));
    bool previous_depth_oob = false;
    bool disoccluded = false;
    if (motion_len > 0.01f)
    {
        float2 display_per_render = float2(g_display_size) / float2(g_render_size);
        history_px = display_px + mv * display_per_render;
        if (history_px.x < 0.0f || history_px.y < 0.0f ||
            history_px.x > float(g_display_size.x - 1) || history_px.y > float(g_display_size.y - 1))
        {
            history_weight = 0.0f;
        }

        float2 previous_render_px = float2(render_px) + mv;
        if (previous_render_px.x < 0.0f || previous_render_px.y < 0.0f ||
            previous_render_px.x > float(g_render_size.x - 1) || previous_render_px.y > float(g_render_size.y - 1))
        {
            previous_depth_oob = true;
            disoccluded = true;
            history_weight = 0.0f;
        }
        else
        {
            previous_depth = SampleRenderFloat(g_previous_depth, previous_render_px);
        }
    }

    if (motion_len > g_motion_rejection_pixels)
    {
        history_weight = 0.0f;
    }
    else if (g_motion_rejection_pixels > 0.0f)
    {
        history_weight *= saturate(1.0f - motion_len / g_motion_rejection_pixels);
    }

    float4 history_color = QuantizeRgba8(SampleDisplay(g_previous_history, history_px));
    float color_residual = abs(Luma(current_color.rgb) - Luma(history_color.rgb));
    if (g_color_rejection_threshold > 0.0f && color_residual > g_color_rejection_threshold)
    {
        history_weight = 0.0f;
    }
    else if (g_color_rejection_threshold > 0.0f)
    {
        history_weight *= saturate(1.0f - color_residual / g_color_rejection_threshold);
    }

    float current_depth = g_current_depth.Load(int3(render_px, 0));
    float depth_residual = previous_depth_oob ? 1.0f : abs(current_depth - previous_depth);
    if (g_depth_rejection_threshold > 0.0f && depth_residual > g_depth_rejection_threshold)
    {
        history_weight = 0.0f;
        disoccluded = true;
    }
    else if (g_depth_rejection_threshold > 0.0f)
    {
        history_weight *= saturate(1.0f - depth_residual / g_depth_rejection_threshold);
    }

    float4 blended = QuantizeRgba8(lerp(current_color, history_color, history_weight));
    float4 resolved = ApplyDetailRecovery(blended, display_px, history_weight, reactive, disoccluded);
    g_output_color[out_px] = QuantizeRgba8(resolved);
}
