cbuffer PassCB : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 ViewProjection;
    float4             ColorMultiplier;
    float4             CameraPos;
    float4             SunDirection;
    float4             SunColor;
    float4             AmbientColor;
    float4             PointLightPos;
    float4             PointLightColor;
    float4             PBRParams;
    row_major float4x4 LightViewProj;
    row_major float4x4 PointLightViewProj;
    float4             ShadowParams;
    float4             PointShadowParams;
    row_major float4x4 CascadeViewProj[4];
    float4             CascadeSplits;
    float4             CascadeSrvIndices;
    float4             CascadeParams;
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

Texture2D   g_Textures[] : register(t0);
SamplerState g_Sampler   : register(s0);

struct VSInput
{
    float3 Position    : POSITION;
    float3 Normal      : NORMAL;
    float2 UV          : TEXCOORD;
    float4 Color       : COLOR;
    uint4  BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 Position     : SV_Position;
    float3 Normal       : NORMAL;
    float2 UV           : TEXCOORD0;
    float4 Color        : COLOR0;
    float3 WorldPos     : POSITION0;
    nointerpolation uint TextureIndex : TEXCOORD1;
    nointerpolation float4 MaterialParams : TEXCOORD2;
};

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData inst = g_Instances[g_BaseInstance + instanceID];
    VSOutput output;

    float3 localPos = input.Position;
    float3 localNormal = input.Normal;

    float weightSum = input.BoneWeights.x + input.BoneWeights.y + input.BoneWeights.z + input.BoneWeights.w;
    if (weightSum > 0.001f)
    {
        float4x4 skinMatrix =
            g_Bones[input.BoneIndices.x] * input.BoneWeights.x +
            g_Bones[input.BoneIndices.y] * input.BoneWeights.y +
            g_Bones[input.BoneIndices.z] * input.BoneWeights.z +
            g_Bones[input.BoneIndices.w] * input.BoneWeights.w;
        localPos = mul(float4(input.Position, 1.0f), skinMatrix).xyz;
        localNormal = mul(input.Normal, (float3x3)skinMatrix);
    }

    float4 worldPos = mul(float4(localPos, 1.0f), inst.WorldMatrix);
    output.Position = mul(worldPos, ViewProjection);
    output.Normal   = mul(float4(localNormal, 0.0f), inst.NormalMatrix).xyz;
    output.UV       = input.UV;
    output.Color    = input.Color * inst.Color;
    output.WorldPos = worldPos.xyz;
    output.TextureIndex = inst.TextureIndex;
    output.MaterialParams = inst.MaterialParams;
    return output;
}

static const float PI = 3.14159265359f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / max(PI * denom * denom, 1e-5f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 1e-5f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

float CalculateDirectionalShadow(float3 worldPos, float3 normal, float3 lightDir, float viewDepth)
{
    if (CascadeParams.w < 0.5f)
    {
        return 1.0f;
    }

    float depth = length(worldPos - CameraPos.xyz);
    if (viewDepth > 1.0f)
    {
        depth = viewDepth;
    }

    if (depth > CascadeSplits.w)
    {
        return 1.0f;
    }

    uint cascadeIdx = 3;
    if (depth < CascadeSplits.x)
    {
        cascadeIdx = 0;
    }
    else if (depth < CascadeSplits.y)
    {
        cascadeIdx = 1;
    }
    else if (depth < CascadeSplits.z)
    {
        cascadeIdx = 2;
    }

    uint shadowMapIdx = asuint(CascadeSrvIndices[cascadeIdx]);
    float bias = CascadeParams.y;
    float res = max(CascadeParams.x, 512.0f);
    float texelSize = 1.0f / res;

    float normalBias = max(bias * 1.5f, 0.001f);
    float4 shadowPos = mul(float4(worldPos + normal * normalBias, 1.0f), CascadeViewProj[cascadeIdx]);
    float3 projCoords = shadowPos.xyz / max(shadowPos.w, 0.0001f);

    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    float currentDepth = projCoords.z;
    float shadow = 0.0f;

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            float sampleDepth = g_Textures[NonUniformResourceIndex(shadowMapIdx)].SampleLevel(g_Sampler, projCoords.xy + offset, 0).r;
            shadow += (currentDepth - bias <= sampleDepth) ? 1.0f : 0.0f;
        }
    }

    return shadow / 9.0f;
}

float CalculatePointShadow(float3 worldPos, float3 normal, float3 lightDir)
{
    if (PointShadowParams.w < 0.5f)
    {
        return 1.0f;
    }

    uint shadowMapIdx = asuint(PointShadowParams.x);
    float bias = PointShadowParams.y;
    float res = max(PointShadowParams.z, 512.0f);
    float texelSize = 1.0f / res;

    float normalBias = max(bias * 1.5f, 0.003f);
    float4 shadowPos = mul(float4(worldPos + normal * normalBias, 1.0f), PointLightViewProj);
    float3 projCoords = shadowPos.xyz / max(shadowPos.w, 0.0001f);

    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    float currentDepth = projCoords.z;
    float shadow = 0.0f;

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            float sampleDepth = g_Textures[NonUniformResourceIndex(shadowMapIdx)].SampleLevel(g_Sampler, projCoords.xy + offset, 0).r;
            shadow += (currentDepth - bias <= sampleDepth) ? 1.0f : 0.0f;
        }
    }

    return shadow / 9.0f;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 texColor = g_Textures[NonUniformResourceIndex(input.TextureIndex)].Sample(g_Sampler, input.UV);
    float4 base = texColor * input.Color * ColorMultiplier;
    float3 albedo = pow(max(base.rgb, 0.0f), 2.2f);

    float metallic = clamp(PBRParams.x, 0.02f, 0.98f);
    float roughness = clamp(PBRParams.y, 0.04f, 1.0f);
    float ao = clamp(PBRParams.z, 0.0f, 1.0f);

    if (input.MaterialParams.w > 0.5f)
    {
        metallic = clamp(input.MaterialParams.x, 0.02f, 0.98f);
        roughness = clamp(input.MaterialParams.y, 0.04f, 1.0f);
        ao = clamp(input.MaterialParams.z, 0.0f, 1.0f);
    }

    float3 N = normalize(input.Normal);
    float3 V = normalize(CameraPos.xyz - input.WorldPos);
    float NdotV = max(dot(N, V), 0.0001f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    float3 L_sun = normalize(SunDirection.xyz);
    float3 H_sun = normalize(V + L_sun);
    float NdotL_sun = max(dot(N, L_sun), 0.0f);
    if (NdotL_sun > 0.0f)
    {
        float shadow = CalculateDirectionalShadow(input.WorldPos, N, L_sun, input.Position.w);
        if (shadow > 0.001f)
        {
            float NDF = DistributionGGX(N, H_sun, roughness);
            float G = GeometrySmith(N, V, L_sun, roughness);
            float3 F = FresnelSchlick(max(dot(H_sun, V), 0.0f), F0);

            float3 numerator = NDF * G * F;
            float denominator = 4.0f * NdotV * NdotL_sun + 0.0001f;
            float3 specular = numerator / denominator;

            float3 kS = F;
            float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);
            float3 radiance = SunColor.rgb * SunDirection.w;

            Lo += (kD * albedo / PI + specular) * radiance * NdotL_sun * shadow;
        }
    }

    float3 pointToLight = PointLightPos.xyz - input.WorldPos;
    float dist = length(pointToLight);
    float range = max(PointLightPos.w, 0.01f);
    if (dist < range)
    {
        float3 L_pt = normalize(pointToLight);
        float3 H_pt = normalize(V + L_pt);
        float NdotL_pt = max(dot(N, L_pt), 0.0f);
        if (NdotL_pt > 0.0f)
        {
            float ptShadow = CalculatePointShadow(input.WorldPos, N, L_pt);
            if (ptShadow > 0.001f)
            {
                float attenuation = pow(saturate(1.0f - (dist / range)), 2.0f) / (1.0f + 0.1f * dist * dist);
                float NDF = DistributionGGX(N, H_pt, roughness);
                float G = GeometrySmith(N, V, L_pt, roughness);
                float3 F = FresnelSchlick(max(dot(H_pt, V), 0.0f), F0);

                float3 numerator = NDF * G * F;
                float denominator = 4.0f * NdotV * NdotL_pt + 0.0001f;
                float3 specular = numerator / denominator;

                float3 kS = F;
                float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);
                float3 radiance = PointLightColor.rgb * PointLightColor.w * attenuation;

                Lo += (kD * albedo / PI + specular) * radiance * NdotL_pt * ptShadow;
            }
        }
    }

    float3 ambient = AmbientColor.rgb * albedo * ao;
    float3 color = ambient + Lo;

    if (PBRParams.w > 0.5f)
    {
        color = ACESFilm(color);
    }
    color = pow(max(color, 0.0f), 1.0f / 2.2f);

    return float4(color, base.a);
}
