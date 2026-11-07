cbuffer PassCB : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 ViewProjection;
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
StructuredBuffer<row_major float4x4> g_Bones : register(t0, space2);

struct VSInput
{
    float3 Position    : POSITION;
    float3 Normal      : NORMAL;
    float2 UV          : TEXCOORD;
    float4 Color       : COLOR;
    uint4  BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

float4 VSMain(VSInput input, uint instanceID : SV_InstanceID) : SV_Position
{
    InstanceData inst = g_Instances[g_BaseInstance + instanceID];

    float3 localPos = input.Position;
    float weightSum = input.BoneWeights.x + input.BoneWeights.y + input.BoneWeights.z + input.BoneWeights.w;
    if (weightSum > 0.001f)
    {
        float4x4 skinMatrix =
            g_Bones[input.BoneIndices.x] * input.BoneWeights.x +
            g_Bones[input.BoneIndices.y] * input.BoneWeights.y +
            g_Bones[input.BoneIndices.z] * input.BoneWeights.z +
            g_Bones[input.BoneIndices.w] * input.BoneWeights.w;
        localPos = mul(float4(input.Position, 1.0f), skinMatrix).xyz;
    }

    float4 worldPos = mul(float4(localPos, 1.0f), inst.WorldMatrix);
    return mul(worldPos, ViewProjection);
}
