Texture2D<float> g_previous_trust : register(t0);
Texture2D<float> g_depth_delta : register(t1);
Texture2D<float2> g_motion_vectors : register(t2);
Texture2D<float> g_reactive : register(t3);
Texture2D<float> g_color_delta_luma : register(t4);
Texture2D<float> g_disocclusion : register(t5);
Texture2D<float> g_reset : register(t6);
RWTexture2D<float> g_history_trust : register(u0);
RWTexture2D<float> g_accumulation_weight : register(u1);

cbuffer TrustConstants : register(b0)
{
    uint2 g_output_size;
    float g_depth_threshold;
    float g_motion_threshold_pixels;
    float g_reactive_penalty;
    float g_disocclusion_penalty;
    float g_trust_decay_rate;
    float g_max_history_weight;
    float g_trust_recovery_floor;
    float g_color_threshold;
    float g_min_history_weight;
};

float saturate_safe(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_output_size.x || dispatch_thread_id.y >= g_output_size.y)
    {
        return;
    }

    uint2 px = dispatch_thread_id.xy;
    float previous = g_previous_trust[px];
    float depth_delta = abs(g_depth_delta[px]);
    float2 mv = g_motion_vectors[px];
    float motion_length = length(mv);
    float reactive = g_reactive[px];
    float color_delta = abs(g_color_delta_luma[px]);
    float disocclusion = saturate_safe(g_disocclusion[px]);
    float reset = g_reset[px];

    if (reset > 0.5f)
    {
        g_history_trust[px] = 0.0f;
        g_accumulation_weight[px] = 0.0f;
        return;
    }

    float depth_trust = saturate_safe(1.0f - depth_delta / max(g_depth_threshold, 0.0001f));
    float motion_trust = saturate_safe(1.0f - motion_length / max(g_motion_threshold_pixels, 0.0001f));
    float color_trust = saturate_safe(1.0f - color_delta / max(g_color_threshold, 0.0001f));
    float reactive_trust = saturate_safe(1.0f - reactive * g_reactive_penalty);
    float disocclusion_trust = saturate_safe(1.0f - disocclusion * g_disocclusion_penalty);
    float decayed = saturate_safe(previous * (1.0f - g_trust_decay_rate));
    float evidence = depth_trust * motion_trust * color_trust * reactive_trust * disocclusion_trust;
    float trust = evidence * max(decayed, saturate_safe(g_trust_recovery_floor));
    float accumulation = trust <= 0.00001f
        ? 0.0f
        : clamp(trust * g_max_history_weight, g_min_history_weight, g_max_history_weight);

    g_history_trust[px] = trust;
    g_accumulation_weight[px] = accumulation;
}
