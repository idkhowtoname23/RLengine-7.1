#pragma once

#include "Graphics/DX12/DX12Common.hpp"

#include <dxcapi.h>
#include <expected>
#include <string>
#include <span>

namespace RL
{
    struct PipelineConfig
    {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        D3D12_CULL_MODE CullMode = D3D12_CULL_MODE_NONE;
        D3D12_FILL_MODE FillMode = D3D12_FILL_MODE_SOLID;
        UINT NumRenderTargets = 1;
        DXGI_FORMAT RtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        bool EnableBlend = false;
        bool EnableDepth = false;
        bool DepthWrite = false;
        D3D12_COMPARISON_FUNC DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;
        INT DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        FLOAT DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        FLOAT SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    };

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        Pipeline(Pipeline&&) noexcept = default;
        Pipeline& operator=(Pipeline&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(
            ID3D12Device* device,
            IDxcBlob* vsBlob,
            IDxcBlob* psBlob,
            std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout = {},
            const PipelineConfig& config = {});

        [[nodiscard]] std::expected<void, std::string> Initialize(
            ID3D12Device* device,
            IDxcBlob* vsBlob,
            IDxcBlob* psBlob,
            std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout,
            DXGI_FORMAT rtvFormat)
        {
            PipelineConfig cfg{};
            cfg.RtvFormat = rtvFormat;
            return Initialize(device, vsBlob, psBlob, inputLayout, cfg);
        }

        void Bind(ID3D12GraphicsCommandList* commandList) const noexcept;

        [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.Get(); }
        [[nodiscard]] ID3D12PipelineState* GetPipelineState() const noexcept { return m_pipelineState.Get(); }

    private:
        [[nodiscard]] std::expected<void, std::string> CreateRootSignature(ID3D12Device* device);
        [[nodiscard]] std::expected<void, std::string> CreatePipelineState(
            ID3D12Device* device,
            IDxcBlob* vsBlob,
            IDxcBlob* psBlob,
            std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout,
            const PipelineConfig& config);

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;
    };
}
