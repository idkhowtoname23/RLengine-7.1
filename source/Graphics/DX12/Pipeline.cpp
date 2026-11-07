#include "Graphics/DX12/Pipeline.hpp"

#include <format>

namespace RL
{
    std::expected<void, std::string> Pipeline::Initialize(
        ID3D12Device* device,
        IDxcBlob* vsBlob,
        IDxcBlob* psBlob,
        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout,
        const PipelineConfig& config)
    {
        if (!device)
        {
            return std::unexpected("Device pointer is null when creating Pipeline.");
        }
        if (!vsBlob)
        {
            return std::unexpected("Vertex shader blob pointer is null when creating Pipeline.");
        }

        auto rootRes = CreateRootSignature(device);
        if (!rootRes)
        {
            return rootRes;
        }

        auto psoRes = CreatePipelineState(device, vsBlob, psBlob, inputLayout, config);
        if (!psoRes)
        {
            return psoRes;
        }

        RL_LOG_INFO("Graphics Pipeline State Object (GPSO) created successfully.");
        return {};
    }

    std::expected<void, std::string> Pipeline::CreateRootSignature(ID3D12Device* device)
    {
        D3D12_ROOT_PARAMETER1 rootParams[5]{};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[0].Descriptor.ShaderRegister = 0;
        rootParams[0].Descriptor.RegisterSpace = 0;
        rootParams[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

        D3D12_DESCRIPTOR_RANGE1 srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1024;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[2].Descriptor.ShaderRegister = 0;
        rootParams[2].Descriptor.RegisterSpace = 1;
        rootParams[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

        rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[3].Constants.ShaderRegister = 1;
        rootParams[3].Constants.RegisterSpace = 0;
        rootParams[3].Constants.Num32BitValues = 1;

        rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[4].Descriptor.ShaderRegister = 0;
        rootParams[4].Descriptor.RegisterSpace = 2;
        rootParams[4].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

        D3D12_STATIC_SAMPLER_DESC staticSampler{};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.MipLODBias = 0.0f;
        staticSampler.MaxAnisotropy = 1;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSampler.MinLOD = 0.0f;
        staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSigDesc.Desc_1_1.NumParameters = 5;
        rootSigDesc.Desc_1_1.pParameters = rootParams;
        rootSigDesc.Desc_1_1.NumStaticSamplers = 1;
        rootSigDesc.Desc_1_1.pStaticSamplers = &staticSampler;
        rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> serializedSignature;
        ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSigDesc, &serializedSignature, &errorBlob);
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                RL_LOG_WARN("Versioned root signature 1.1 serialization failed: {}",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }

            D3D12_DESCRIPTOR_RANGE srvRange1_0{};
            srvRange1_0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange1_0.NumDescriptors = 1;
            srvRange1_0.BaseShaderRegister = 0;
            srvRange1_0.RegisterSpace = 0;
            srvRange1_0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER rootParams1_0[5]{};
            rootParams1_0[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParams1_0[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            rootParams1_0[0].Descriptor.ShaderRegister = 0;
            rootParams1_0[0].Descriptor.RegisterSpace = 0;

            rootParams1_0[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParams1_0[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            rootParams1_0[1].DescriptorTable.NumDescriptorRanges = 1;
            rootParams1_0[1].DescriptorTable.pDescriptorRanges = &srvRange1_0;

            rootParams1_0[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            rootParams1_0[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            rootParams1_0[2].Descriptor.ShaderRegister = 0;
            rootParams1_0[2].Descriptor.RegisterSpace = 1;

            rootParams1_0[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rootParams1_0[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            rootParams1_0[3].Constants.ShaderRegister = 1;
            rootParams1_0[3].Constants.RegisterSpace = 0;
            rootParams1_0[3].Constants.Num32BitValues = 1;

            rootParams1_0[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            rootParams1_0[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            rootParams1_0[4].Descriptor.ShaderRegister = 0;
            rootParams1_0[4].Descriptor.RegisterSpace = 2;

            D3D12_ROOT_SIGNATURE_DESC rootSig1_0{};
            rootSig1_0.NumParameters = 5;
            rootSig1_0.pParameters = rootParams1_0;
            rootSig1_0.NumStaticSamplers = 1;
            rootSig1_0.pStaticSamplers = &staticSampler;
            rootSig1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            RL_CHECK_HR(D3D12SerializeRootSignature(&rootSig1_0, D3D_ROOT_SIGNATURE_VERSION_1, &serializedSignature, &errorBlob));
        }

        RL_CHECK_HR(device->CreateRootSignature(
            0,
            serializedSignature->GetBufferPointer(),
            serializedSignature->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature)
        ));

        return {};
    }

    std::expected<void, std::string> Pipeline::CreatePipelineState(
        ID3D12Device* device,
        IDxcBlob* vsBlob,
        IDxcBlob* psBlob,
        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout,
        const PipelineConfig& config)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = psBlob ? D3D12_SHADER_BYTECODE{ psBlob->GetBufferPointer(), psBlob->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };

        psoDesc.RasterizerState.FillMode = config.FillMode;
        psoDesc.RasterizerState.CullMode = config.CullMode;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = config.DepthBias;
        psoDesc.RasterizerState.DepthBiasClamp = config.DepthBiasClamp;
        psoDesc.RasterizerState.SlopeScaledDepthBias = config.SlopeScaledDepthBias;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};
        rtBlendDesc.BlendEnable = config.EnableBlend ? TRUE : FALSE;
        rtBlendDesc.LogicOpEnable = FALSE;
        rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
        rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            psoDesc.BlendState.RenderTarget[i] = rtBlendDesc;
        }

        psoDesc.DepthStencilState.DepthEnable = config.EnableDepth ? TRUE : FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = config.DepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = config.DepthFunc;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DSVFormat = config.EnableDepth ? config.DsvFormat : DXGI_FORMAT_UNKNOWN;

        if (!inputLayout.empty())
        {
            psoDesc.InputLayout = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
        }
        else
        {
            psoDesc.InputLayout = { nullptr, 0 };
        }

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = config.TopologyType;
        psoDesc.NumRenderTargets = config.NumRenderTargets;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            psoDesc.RTVFormats[i] = (i < config.NumRenderTargets) ? config.RtvFormat : DXGI_FORMAT_UNKNOWN;
        }
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        RL_CHECK_HR(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        return {};
    }

    void Pipeline::Bind(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        if (!commandList) return;

        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        commandList->SetPipelineState(m_pipelineState.Get());
    }
}
