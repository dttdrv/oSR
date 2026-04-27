Texture2D<float> g_previous_trust : register(t0);
Texture2D<float> g_depth_delta : register(t1);
Texture2D<float2> g_motion_vectors : register(t2);
Texture2D<float> g_reactive : register(t3);
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

    float depth_trust = saturate_safe(1.0f - depth_delta / max(g_depth_threshold, 0.0001f));
    float motion_trust = saturate_safe(1.0f - motion_length / max(g_motion_threshold_pixels, 0.0001f));
    float reactive_trust = saturate_safe(1.0f - reactive * g_reactive_penalty);
    float decayed = saturate_safe(previous * (1.0f - g_trust_decay_rate));
    float trust = decayed * depth_trust * motion_trust * reactive_trust;

    g_history_trust[px] = trust;
    g_accumulation_weight[px] = saturate_safe(trust * g_max_history_weight);
}

