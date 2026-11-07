cbuffer ObjectCB : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 ViewProjection;
    float4             ColorMultiplier;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD;
    float4 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
    float3 WorldPos : POSITION_WORLD;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.Position, 1.0f), WorldMatrix);
    output.Position = mul(worldPos, ViewProjection);
    output.Color    = input.Color * ColorMultiplier;
    output.WorldPos = worldPos.xyz;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float dist = length(input.WorldPos.xz);
    float fade = saturate(1.0f - (dist / 95.0f));
    fade = fade * fade;
    return float4(input.Color.rgb, input.Color.a * fade);
}
