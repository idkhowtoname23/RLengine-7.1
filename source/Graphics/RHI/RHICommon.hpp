#pragma once

#include "Core/Types.hpp"
#include <string>
#include <string_view>

namespace RL::RHI
{
    enum class GraphicsAPI : u8
    {
        DirectX12 = 0,
        DirectX11 = 1,
    };

    enum class GpuVendor : u8
    {
        Unknown = 0,
        Nvidia  = 1,
        AMD     = 2,
        Intel   = 3,
        Software= 4
    };

    enum class GpuGeneration : u16
    {
        Unknown = 0,

        Nvidia_Fermi,
        Nvidia_Kepler,
        Nvidia_Maxwell,
        Nvidia_Pascal,
        Nvidia_Turing,
        Nvidia_Ampere,
        Nvidia_AdaLovelace,
        Nvidia_Blackwell,

        AMD_GCN,
        AMD_RDNA1,
        AMD_RDNA2,
        AMD_RDNA3,
        AMD_RDNA4,

        Intel_Gen9,
        Intel_Iris,
        Intel_Arc_Alchemist,
        Intel_Arc_Battlemage,

        Generic
    };

    enum class D3DFeatureLevel : u8
    {
        Level_11_0 = 0,
        Level_11_1 = 1,
        Level_12_0 = 2,
        Level_12_1 = 3,
        Level_12_2 = 4,
    };

    enum class ShaderModelVersion : u8
    {
        SM_5_0 = 0,
        SM_5_1 = 1,
        SM_6_0 = 2,
        SM_6_1 = 3,
        SM_6_2 = 4,
        SM_6_3 = 5,
        SM_6_4 = 6,
        SM_6_5 = 7,
        SM_6_6 = 8,
        SM_6_7 = 9,
        SM_6_8 = 10,
    };

    struct GpuDeviceInfo
    {
        std::string name;
        std::string vendorName;
        std::string generationName;
        std::string architectureName;

        GpuVendor vendor = GpuVendor::Unknown;
        GpuGeneration generation = GpuGeneration::Unknown;
        D3DFeatureLevel maxFeatureLevel = D3DFeatureLevel::Level_11_0;
        ShaderModelVersion maxShaderModel = ShaderModelVersion::SM_5_0;

        u64 dedicatedVramBytes = 0;
        u64 sharedSystemMemoryBytes = 0;
        u32 vendorId = 0;
        u32 deviceId = 0;

        bool supportsD3D12 = false;
        bool supportsD3D12Ultimate = false;
        bool supportsRaytracing = false;
        bool supportsMeshShaders = false;
        bool supportsVariableRateShading = false;
        bool supportsTearing = false;

        [[nodiscard]] f64 GetDedicatedVramGB() const noexcept
        {
            return static_cast<f64>(dedicatedVramBytes) / (1024.0 * 1024.0 * 1024.0);
        }

        [[nodiscard]] std::string_view GetFeatureLevelString() const noexcept
        {
            switch (maxFeatureLevel)
            {
                case D3DFeatureLevel::Level_11_0: return "D3D_FEATURE_LEVEL_11_0";
                case D3DFeatureLevel::Level_11_1: return "D3D_FEATURE_LEVEL_11_1";
                case D3DFeatureLevel::Level_12_0: return "D3D_FEATURE_LEVEL_12_0";
                case D3DFeatureLevel::Level_12_1: return "D3D_FEATURE_LEVEL_12_1";
                case D3DFeatureLevel::Level_12_2: return "D3D_FEATURE_LEVEL_12_2 (Ultimate)";
                default: return "Unknown";
            }
        }

        [[nodiscard]] std::string_view GetShaderModelString() const noexcept
        {
            switch (maxShaderModel)
            {
                case ShaderModelVersion::SM_5_0: return "Shader Model 5.0";
                case ShaderModelVersion::SM_5_1: return "Shader Model 5.1";
                case ShaderModelVersion::SM_6_0: return "Shader Model 6.0";
                case ShaderModelVersion::SM_6_1: return "Shader Model 6.1";
                case ShaderModelVersion::SM_6_2: return "Shader Model 6.2";
                case ShaderModelVersion::SM_6_3: return "Shader Model 6.3";
                case ShaderModelVersion::SM_6_4: return "Shader Model 6.4";
                case ShaderModelVersion::SM_6_5: return "Shader Model 6.5";
                case ShaderModelVersion::SM_6_6: return "Shader Model 6.6";
                case ShaderModelVersion::SM_6_7: return "Shader Model 6.7";
                case ShaderModelVersion::SM_6_8: return "Shader Model 6.8";
                default: return "Unknown";
            }
        }
    };
}
