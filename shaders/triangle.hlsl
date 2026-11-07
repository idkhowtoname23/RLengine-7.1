
struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;

    const float2 positions[3] = {
        float2( 0.0f,  0.55f),
        float2( 0.5f, -0.45f),
        float2(-0.5f, -0.45f)
    };

    const float4 colors[3] = {
        float4(1.0f,  0.22f, 0.38f, 1.0f),
        float4(0.18f, 0.95f, 0.52f, 1.0f),
        float4(0.24f, 0.58f, 1.0f,  1.0f)
    };

    output.position = float4(positions[vertexID], 0.0f, 1.0f);
    output.color = colors[vertexID];
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return input.color;
}
