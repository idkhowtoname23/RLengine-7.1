cbuffer PassCB : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 ViewProjection;
    float4             ColorMultiplier;
};

cbuffer DrawCB : register(b1)
{
    uint g_BaseInstance;
};

struct InstanceData
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 NormalMatrix;
    float4             Color;
    float4             MaterialParams;
    uint               TextureIndex;
    uint3              Padding;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space1);

Texture2D   g_Textures[] : register(t0);
SamplerState g_Sampler   : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
};

struct VSOutput
{
    float4 Position     : SV_Position;
    float4 Color        : COLOR0;
    float2 UV           : TEXCOORD0;
    nointerpolation uint TextureIndex : TEXCOORD1;
};

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData inst = g_Instances[g_BaseInstance + instanceID];
    VSOutput output;
    float4 worldPos = mul(float4(input.Position, 1.0f), inst.WorldMatrix);
    output.Position = mul(worldPos, ViewProjection);
    output.Color    = input.Color * inst.Color;
    output.UV       = input.UV;
    output.TextureIndex = inst.TextureIndex;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 texColor = g_Textures[NonUniformResourceIndex(input.TextureIndex)].Sample(g_Sampler, input.UV);
    return texColor * input.Color;
}
