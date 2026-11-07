cbuffer PassCB : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 ViewProjection;
    float4             ColorMultiplier;
};

struct ParticleInstance
{
    float4 PositionSize;
    float4 Color;
};

StructuredBuffer<ParticleInstance> g_Instances : register(t0, space1);

struct VSInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORD;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    ParticleInstance inst = g_Instances[instanceID];
    float3 quadPos = input.Position * inst.PositionSize.w;
    float3 worldPos = inst.PositionSize.xyz + quadPos;

    VSOutput output;
    output.Position = mul(float4(worldPos, 1.0f), ViewProjection);
    output.UV = input.UV;
    output.Color = inst.Color * ColorMultiplier;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float2 d = input.UV - float2(0.5f, 0.5f);
    float distSq = dot(d, d);
    if (distSq > 0.25f)
    {
        discard;
    }
    float alpha = smoothstep(0.25f, 0.0f, distSq) * input.Color.a;
    return float4(input.Color.rgb, alpha);
}
