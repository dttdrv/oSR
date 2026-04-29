Texture2D<float4> g_current_color : register(t0);
Texture2D<float4> g_previous_history : register(t1);
Texture2D<float> g_current_depth : register(t2);
Texture2D<float> g_previous_depth : register(t3);
Texture2D<float2> g_motion_vectors : register(t4);
Texture2D<float> g_reactive_mask : register(t5);
RWTexture2D<float4> g_output_color : register(u0);
RWTexture2D<float> g_debug_history_weight : register(u1);
RWTexture2D<float> g_debug_color_residual : register(u2);
RWTexture2D<float> g_debug_depth_residual : register(u3);

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
    float g_history_clip_margin;
    float g_feature_lock_sharpening_boost;
    float2 g_jitter_offset;
};

float Luma(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 ToYCoCg(float3 c)
{
    return float3(c.r * 0.25f + c.g * 0.5f + c.b * 0.25f,
                  c.r * 0.5f - c.b * 0.5f,
                  -c.r * 0.25f + c.g * 0.5f - c.b * 0.25f);
}

float3 FromYCoCg(float3 c)
{
    return float3(c.x + c.y - c.z,
                  c.x + c.z,
                  c.x - c.y - c.z);
}

float4 QuantizeRgba8(float4 c)
{
    return floor(saturate(c) * 255.0f + 0.5f) / 255.0f;
}

float4 SampleRenderColor(Texture2D<float4> texture_source, float2 p);

float4 SampleCurrentDisplay(float2 display_px)
{
    float2 render_float = ((display_px + 0.5f) * float2(g_render_size) / float2(g_display_size)) - 0.5f - g_jitter_offset;
    return QuantizeRgba8(SampleRenderColor(g_current_color, render_float));
}

float LocalEdgeStrength(float2 display_px)
{
    float2 left_px = float2(max(display_px.x - 1.0f, 0.0f), display_px.y);
    float2 right_px = float2(min(display_px.x + 1.0f, float(g_display_size.x - 1)), display_px.y);
    float2 up_px = float2(display_px.x, max(display_px.y - 1.0f, 0.0f));
    float2 down_px = float2(display_px.x, min(display_px.y + 1.0f, float(g_display_size.y - 1)));
    float dx = abs(Luma(SampleCurrentDisplay(right_px).rgb) - Luma(SampleCurrentDisplay(left_px).rgb));
    float dy = abs(Luma(SampleCurrentDisplay(down_px).rgb) - Luma(SampleCurrentDisplay(up_px).rgb));
    return saturate(max(dx, dy));
}

float FeatureLockStrength(float2 display_px, float history_weight, float color_residual, float motion_len, float reactive, bool disoccluded)
{
    bool stable = LocalEdgeStrength(display_px) >= 0.18f &&
                  history_weight >= 0.70f &&
                  color_residual <= 0.045f &&
                  (color_residual * color_residual) <= 0.0008f &&
                  motion_len <= 1.5f &&
                  reactive < 0.20f &&
                  !disoccluded;
    return stable ? 0.22f : 0.0f;
}

float4 ApplyDetailRecovery(float4 resolved, float2 display_px, float history_weight, float feature_lock_strength, float reactive, bool disoccluded)
{
    if (g_sharpening_amount <= 0.0f || disoccluded)
    {
        return resolved;
    }

    float trust_scale = lerp(saturate(g_sharpening_low_trust_scale), 1.0f, saturate(history_weight));
    float reactive_scale = lerp(1.0f, saturate(g_sharpening_reactive_scale), saturate(reactive));
    float lock_scale = 1.0f + saturate(feature_lock_strength) * saturate(g_feature_lock_sharpening_boost);
    float amount = saturate(g_sharpening_amount) * trust_scale * reactive_scale * lock_scale;
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

float4 ClipHistoryToCurrentNeighborhood(float4 history_color, float2 display_px)
{
    if (g_history_clip_margin <= 0.0f)
    {
        return history_color;
    }

    float2 left_px = float2(max(display_px.x - 1.0f, 0.0f), display_px.y);
    float2 right_px = float2(min(display_px.x + 1.0f, float(g_display_size.x - 1)), display_px.y);
    float2 up_px = float2(display_px.x, max(display_px.y - 1.0f, 0.0f));
    float2 down_px = float2(display_px.x, min(display_px.y + 1.0f, float(g_display_size.y - 1)));
    float3 c0 = SampleCurrentDisplay(display_px).rgb;
    float3 c1 = SampleCurrentDisplay(left_px).rgb;
    float3 c2 = SampleCurrentDisplay(right_px).rgb;
    float3 c3 = SampleCurrentDisplay(up_px).rgb;
    float3 c4 = SampleCurrentDisplay(down_px).rgb;
    float3 y0 = ToYCoCg(c0);
    float3 y1 = ToYCoCg(c1);
    float3 y2 = ToYCoCg(c2);
    float3 y3 = ToYCoCg(c3);
    float3 y4 = ToYCoCg(c4);
    float3 lo = min(y0, min(y1, min(y2, min(y3, y4))));
    float3 hi = max(y0, max(y1, max(y2, max(y3, y4))));
    float3 history_ycocg = ToYCoCg(history_color.rgb);
    history_ycocg = clamp(history_ycocg, lo - g_history_clip_margin, hi + g_history_clip_margin);
    history_color.rgb = saturate(FromYCoCg(history_ycocg));
    return QuantizeRgba8(history_color);
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

    float4 history_color = ClipHistoryToCurrentNeighborhood(QuantizeRgba8(SampleDisplay(g_previous_history, history_px)), display_px);
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

    float feature_lock_strength = FeatureLockStrength(display_px, history_weight, color_residual, motion_len, reactive, disoccluded);
    float4 blended = QuantizeRgba8(lerp(current_color, history_color, history_weight));
    float4 resolved = ApplyDetailRecovery(blended, display_px, history_weight, feature_lock_strength, reactive, disoccluded);
    g_output_color[out_px] = QuantizeRgba8(resolved);
    g_debug_history_weight[out_px] = history_weight;
    g_debug_color_residual[out_px] = color_residual;
    g_debug_depth_residual[out_px] = depth_residual;
}
