#pragma once

#include "Graphics/RHI/RHICommon.hpp"
#include "Core/Logger.hpp"

#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>

namespace RL::RHI
{
    using Microsoft::WRL::ComPtr;

    class GpuCapabilities
    {
    public:
        static std::vector<GpuDeviceInfo> EnumerateAdapters()
        {
            std::vector<GpuDeviceInfo> devices;

            ComPtr<IDXGIFactory4> factory;
            HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
            if (FAILED(hr))
            {
                RL_LOG_ERROR("GpuCapabilities: failed to create DXGIFactory1");
                return devices;
            }

            for (UINT i = 0;; ++i)
            {
                ComPtr<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }

                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                GpuDeviceInfo info = AnalyzeAdapter(adapter.Get(), desc);
                devices.push_back(std::move(info));
            }

            return devices;
        }

        static GpuDeviceInfo DetectPrimaryGpu()
        {
            auto devices = EnumerateAdapters();
            if (devices.empty())
            {
                GpuDeviceInfo fallback{};
                fallback.name = "Unknown Video Controller";
                fallback.generationName = "Generic Direct3D 11";
                return fallback;
            }

            auto best = std::max_element(devices.begin(), devices.end(), [](const GpuDeviceInfo& a, const GpuDeviceInfo& b) {
                return a.dedicatedVramBytes < b.dedicatedVramBytes;
            });

            return *best;
        }

    private:
        static GpuDeviceInfo AnalyzeAdapter(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc)
        {
            GpuDeviceInfo info{};
            info.dedicatedVramBytes = desc.DedicatedVideoMemory;
            info.sharedSystemMemoryBytes = desc.SharedSystemMemory;
            info.vendorId = desc.VendorId;
            info.deviceId = desc.DeviceId;

            char nameBuf[256]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
            info.name = nameBuf;

            switch (desc.VendorId)
            {
                case 0x10DE:
                    info.vendor = GpuVendor::Nvidia;
                    info.vendorName = "NVIDIA";
                    break;
                case 0x1002:
                    info.vendor = GpuVendor::AMD;
                    info.vendorName = "AMD";
                    break;
                case 0x8086:
                    info.vendor = GpuVendor::Intel;
                    info.vendorName = "Intel";
                    break;
                default:
                    info.vendor = GpuVendor::Unknown;
                    info.vendorName = "Generic";
                    break;
            }

            std::string upperName = info.name;
            std::transform(upperName.begin(), upperName.end(), upperName.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });

            if (info.vendor == GpuVendor::Nvidia)
            {
                ClassifyNvidiaGeneration(upperName, info);
            }
            else if (info.vendor == GpuVendor::AMD)
            {
                ClassifyAmdGeneration(upperName, info);
            }
            else if (info.vendor == GpuVendor::Intel)
            {
                ClassifyIntelGeneration(upperName, info);
            }

            D3D_FEATURE_LEVEL d3d11FeatureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0
            };
            D3D_FEATURE_LEVEL obtainedD3D11Level{};
            ComPtr<ID3D11Device> d3d11Device;
                        HRESULT hr11 = D3D11CreateDevice(
                adapter,
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                0,
                d3d11FeatureLevels,
                static_cast<UINT>(std::size(d3d11FeatureLevels)),
                D3D11_SDK_VERSION,
                &d3d11Device,
                &obtainedD3D11Level,
                nullptr
            );

            if (SUCCEEDED(hr11))
            {
                if (obtainedD3D11Level >= D3D_FEATURE_LEVEL_11_1)
                    info.maxFeatureLevel = D3DFeatureLevel::Level_11_1;
                else
                    info.maxFeatureLevel = D3DFeatureLevel::Level_11_0;

                info.maxShaderModel = ShaderModelVersion::SM_5_0;
            }

            ComPtr<ID3D12Device> d3d12Device;
            D3D_FEATURE_LEVEL d3d12LevelsToTest[] = {
                static_cast<D3D_FEATURE_LEVEL>(0xcc00),
                D3D_FEATURE_LEVEL_12_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0
            };

            for (auto level : d3d12LevelsToTest)
            {
                if (SUCCEEDED(D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&d3d12Device))))
                {
                    info.supportsD3D12 = true;
                    if (level == static_cast<D3D_FEATURE_LEVEL>(0xcc00))
                    {
                        info.maxFeatureLevel = D3DFeatureLevel::Level_12_2;
                        info.supportsD3D12Ultimate = true;
                    }
                    else if (level == D3D_FEATURE_LEVEL_12_1)
                        info.maxFeatureLevel = D3DFeatureLevel::Level_12_1;
                    else if (level == D3D_FEATURE_LEVEL_12_0)
                        info.maxFeatureLevel = D3DFeatureLevel::Level_12_0;
                    break;
                }
            }

            if (d3d12Device)
            {

                D3D12_FEATURE_DATA_SHADER_MODEL smData{};
                D3D_SHADER_MODEL smToTest[] = {
                    static_cast<D3D_SHADER_MODEL>(0x0068),
                    static_cast<D3D_SHADER_MODEL>(0x0067),
                    D3D_SHADER_MODEL_6_6,
                    D3D_SHADER_MODEL_6_5,
                    D3D_SHADER_MODEL_6_4,
                    D3D_SHADER_MODEL_6_3,
                    D3D_SHADER_MODEL_6_2,
                    D3D_SHADER_MODEL_6_1,
                    D3D_SHADER_MODEL_6_0,
                    D3D_SHADER_MODEL_5_1
                };

                for (auto targetSm : smToTest)
                {
                    smData.HighestShaderModel = targetSm;
                    if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &smData, sizeof(smData))))
                    {
                        if (smData.HighestShaderModel >= static_cast<D3D_SHADER_MODEL>(0x0068))
                            info.maxShaderModel = ShaderModelVersion::SM_6_8;
                        else if (smData.HighestShaderModel >= static_cast<D3D_SHADER_MODEL>(0x0067))
                            info.maxShaderModel = ShaderModelVersion::SM_6_7;
                        else if (smData.HighestShaderModel >= D3D_SHADER_MODEL_6_6)
                            info.maxShaderModel = ShaderModelVersion::SM_6_6;
                        else if (smData.HighestShaderModel >= D3D_SHADER_MODEL_6_5)
                            info.maxShaderModel = ShaderModelVersion::SM_6_5;
                        else if (smData.HighestShaderModel >= D3D_SHADER_MODEL_6_0)
                            info.maxShaderModel = ShaderModelVersion::SM_6_0;
                        else
                            info.maxShaderModel = ShaderModelVersion::SM_5_1;
                        break;
                    }
                }

                D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
                if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5))))
                {
                    info.supportsRaytracing = (opt5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
                }

                D3D12_FEATURE_DATA_D3D12_OPTIONS7 opt7{};
                if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &opt7, sizeof(opt7))))
                {
                    info.supportsMeshShaders = (opt7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);
                }

                D3D12_FEATURE_DATA_D3D12_OPTIONS6 opt6{};
                if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &opt6, sizeof(opt6))))
                {
                    info.supportsVariableRateShading = (opt6.VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED);
                }
            }

            return info;
        }

        static void ClassifyNvidiaGeneration(std::string_view name, GpuDeviceInfo& info)
        {

            if (name.find("5090") != std::string_view::npos ||
                name.find("5080") != std::string_view::npos ||
                name.find("5070") != std::string_view::npos ||
                name.find("5060") != std::string_view::npos ||
                name.find("5050") != std::string_view::npos ||
                name.find("RTX 50") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Blackwell;
                info.generationName = "GeForce RTX 5000 Series";
                info.architectureName = "Blackwell (GB20x)";
                info.supportsD3D12Ultimate = true;
                return;
            }

            if (name.find("4090") != std::string_view::npos ||
                name.find("4080") != std::string_view::npos ||
                name.find("4070") != std::string_view::npos ||
                name.find("4060") != std::string_view::npos ||
                name.find("RTX 40") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_AdaLovelace;
                info.generationName = "GeForce RTX 4000 Series";
                info.architectureName = "Ada Lovelace (AD10x)";
                info.supportsD3D12Ultimate = true;
                return;
            }

            if (name.find("3090") != std::string_view::npos ||
                name.find("3080") != std::string_view::npos ||
                name.find("3070") != std::string_view::npos ||
                name.find("3060") != std::string_view::npos ||
                name.find("3050") != std::string_view::npos ||
                name.find("RTX 30") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Ampere;
                info.generationName = "GeForce RTX 3000 Series";
                info.architectureName = "Ampere (GA10x)";
                info.supportsD3D12Ultimate = true;
                return;
            }

            if (name.find("2080") != std::string_view::npos ||
                name.find("2070") != std::string_view::npos ||
                name.find("2060") != std::string_view::npos ||
                name.find("1660") != std::string_view::npos ||
                name.find("1650") != std::string_view::npos ||
                name.find("RTX 20") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Turing;
                info.generationName = "GeForce RTX 2000 / GTX 1600 Series";
                info.architectureName = "Turing (TU10x/TU11x)";
                return;
            }

            if (name.find("1080") != std::string_view::npos ||
                name.find("1070") != std::string_view::npos ||
                name.find("1060") != std::string_view::npos ||
                name.find("1050") != std::string_view::npos ||
                name.find("GTX 10") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Pascal;
                info.generationName = "GeForce GTX 1000 Series";
                info.architectureName = "Pascal (GP10x)";
                return;
            }

            if (name.find("980") != std::string_view::npos ||
                name.find("970") != std::string_view::npos ||
                name.find("960") != std::string_view::npos ||
                name.find("950") != std::string_view::npos ||
                name.find("750") != std::string_view::npos ||
                name.find("GTX 9") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Maxwell;
                info.generationName = "GeForce GTX 900 Series";
                info.architectureName = "Maxwell (GM20x)";
                return;
            }

            if (name.find("780") != std::string_view::npos ||
                name.find("770") != std::string_view::npos ||
                name.find("760") != std::string_view::npos ||
                name.find("680") != std::string_view::npos ||
                name.find("670") != std::string_view::npos ||
                name.find("660") != std::string_view::npos ||
                name.find("GTX 6") != std::string_view::npos ||
                name.find("GTX 7") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Kepler;
                info.generationName = "GeForce GTX 600/700 Series";
                info.architectureName = "Kepler (GK10x)";
                return;
            }

            if (name.find("590") != std::string_view::npos ||
                name.find("580") != std::string_view::npos ||
                name.find("570") != std::string_view::npos ||
                name.find("560") != std::string_view::npos ||
                name.find("550") != std::string_view::npos ||
                name.find("520") != std::string_view::npos ||
                name.find("480") != std::string_view::npos ||
                name.find("470") != std::string_view::npos ||
                name.find("460") != std::string_view::npos ||
                name.find("GTX 5") != std::string_view::npos ||
                name.find("GTX 4") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Nvidia_Fermi;
                info.generationName = "GeForce GTX 500 Series";
                info.architectureName = "Fermi (GF10x/GF11x)";
                return;
            }

            info.generation = GpuGeneration::Generic;
            info.generationName = "NVIDIA GeForce GPU";
            info.architectureName = "NVIDIA Modern GPU";
        }

        static void ClassifyAmdGeneration(std::string_view name, GpuDeviceInfo& info)
        {
            if (name.find("7900") != std::string_view::npos || name.find("7800") != std::string_view::npos || name.find("7700") != std::string_view::npos || name.find("7600") != std::string_view::npos)
            {
                info.generation = GpuGeneration::AMD_RDNA3;
                info.generationName = "Radeon RX 7000 Series";
                info.architectureName = "RDNA 3";
                info.supportsD3D12Ultimate = true;
            }
            else if (name.find("6900") != std::string_view::npos || name.find("6800") != std::string_view::npos || name.find("6700") != std::string_view::npos || name.find("6600") != std::string_view::npos)
            {
                info.generation = GpuGeneration::AMD_RDNA2;
                info.generationName = "Radeon RX 6000 Series";
                info.architectureName = "RDNA 2";
                info.supportsD3D12Ultimate = true;
            }
            else
            {
                info.generation = GpuGeneration::AMD_GCN;
                info.generationName = "Radeon Graphics";
                info.architectureName = "AMD Graphics Core Next";
            }
        }

        static void ClassifyIntelGeneration(std::string_view name, GpuDeviceInfo& info)
        {
            if (name.find("ARC") != std::string_view::npos)
            {
                info.generation = GpuGeneration::Intel_Arc_Alchemist;
                info.generationName = "Intel Arc Graphics";
                info.architectureName = "Xe-HPG";
                info.supportsD3D12Ultimate = true;
            }
            else
            {
                info.generation = GpuGeneration::Intel_Gen9;
                info.generationName = "Intel HD/UHD/Iris Graphics";
                info.architectureName = "Intel Gen Graphics";
            }
        }
    };
}
