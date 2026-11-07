#pragma once

#include "Core/Types.hpp"
#include <DirectXMath.h>

namespace RL
{
    struct Vertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT4 Color;
        DirectX::XMFLOAT2 UV;
    };

    struct alignas(256) ObjectConstants
    {
        DirectX::XMFLOAT4X4 WorldMatrix;
        DirectX::XMFLOAT4X4 ViewProjection;
        DirectX::XMFLOAT4   ColorMultiplier{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4   CameraPos{ 0.0f, 0.0f, -10.0f, 1.0f };
        DirectX::XMFLOAT4   SunDirection{ 0.5f, 0.8f, -0.7f, 3.0f };
        DirectX::XMFLOAT4   SunColor{ 1.0f, 0.98f, 0.92f, 1.0f };
        DirectX::XMFLOAT4   AmbientColor{ 0.15f, 0.18f, 0.22f, 1.0f };
        DirectX::XMFLOAT4   PointLightPos{ 0.0f, 3.0f, 0.0f, 15.0f };
        DirectX::XMFLOAT4   PointLightColor{ 1.0f, 0.85f, 0.6f, 5.0f };
        DirectX::XMFLOAT4   PBRParams{ 0.2f, 0.4f, 1.0f, 1.0f };
        DirectX::XMFLOAT4X4 LightViewProj;
        DirectX::XMFLOAT4X4 PointLightViewProj;
        DirectX::XMFLOAT4   ShadowParams{ 0.0f, 0.002f, 2048.0f, 1.0f };
        DirectX::XMFLOAT4   PointShadowParams{ 0.0f, 0.003f, 1024.0f, 1.0f };
        DirectX::XMFLOAT4X4 CascadeViewProj[4];
        DirectX::XMFLOAT4   CascadeSplits{ 10.0f, 25.0f, 60.0f, 150.0f };
        DirectX::XMFLOAT4   CascadeSrvIndices{ 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4   CascadeParams{ 2048.0f, 0.0012f, 4.0f, 1.0f };
        u8                  Padding[1024 - (sizeof(DirectX::XMFLOAT4X4) * 8 + sizeof(DirectX::XMFLOAT4) * 13)];
    };
    static_assert(sizeof(ObjectConstants) == 1024, "ObjectConstants must be exactly 1024 bytes aligned");

    struct alignas(256) FrameConstants
    {
        DirectX::XMFLOAT4X4 ViewProjection;
        u8                  Padding[256 - sizeof(DirectX::XMFLOAT4X4)];
    };
    static_assert(sizeof(FrameConstants) == 256, "FrameConstants must be exactly 256 bytes aligned");

    struct alignas(16) InstanceData
    {
        DirectX::XMFLOAT4X4 WorldMatrix;
        DirectX::XMFLOAT4X4 NormalMatrix;
        DirectX::XMFLOAT4   Color;
        DirectX::XMFLOAT4   MaterialParams{ 0.0f, 0.5f, 1.0f, 0.0f };
        u32                 TextureIndex = 0;
        u32                 Padding[3]{};
    };
    static_assert(sizeof(InstanceData) == 176, "InstanceData must be exactly 176 bytes");

    using ObjectData = ObjectConstants;
}
