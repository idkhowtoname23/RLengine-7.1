#pragma once

#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/DX12/DescriptorManager.hpp"

#include <expected>
#include <string>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace RL
{
    class ShadowMap
    {
    public:
        static constexpr u32 CascadeCount = 4;
        static constexpr u32 DirectionalResolution = 2048;
        static constexpr u32 PointResolution = 1024;

        ShadowMap() = default;
        ~ShadowMap();

        ShadowMap(const ShadowMap&) = delete;
        ShadowMap& operator=(const ShadowMap&) = delete;

        ShadowMap(ShadowMap&&) noexcept = default;
        ShadowMap& operator=(ShadowMap&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(ID3D12Device* device, DescriptorManager* descriptorManager);
        void Destroy() noexcept;

        void BeginCascade(ID3D12GraphicsCommandList* cmdList, u32 cascadeIndex);
        void EndCascade(ID3D12GraphicsCommandList* cmdList, u32 cascadeIndex);

        void BeginDirectional(ID3D12GraphicsCommandList* cmdList);
        void EndDirectional(ID3D12GraphicsCommandList* cmdList);

        void BeginPoint(ID3D12GraphicsCommandList* cmdList);
        void EndPoint(ID3D12GraphicsCommandList* cmdList);

        [[nodiscard]] u32 GetCascadeSrvIndex(u32 cascadeIndex) const noexcept;
        [[nodiscard]] u32 GetDirectionalSrvIndex() const noexcept { return GetCascadeSrvIndex(0); }
        [[nodiscard]] u32 GetPointSrvIndex() const noexcept { return m_pointSrvIndex; }
        [[nodiscard]] u32 GetDirectionalResolution() const noexcept { return DirectionalResolution; }
        [[nodiscard]] u32 GetPointResolution() const noexcept { return PointResolution; }

    private:
        ComPtr<ID3D12Resource> m_cascadeShadowMaps[CascadeCount];
        ComPtr<ID3D12Resource> m_pointShadowMap;
        ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

        D3D12_CPU_DESCRIPTOR_HANDLE m_cascadeDsvHandles[CascadeCount]{};
        D3D12_CPU_DESCRIPTOR_HANDLE m_pointDsvHandle{};

        D3D12_VIEWPORT m_dirViewport{};
        D3D12_RECT m_dirScissorRect{};

        D3D12_VIEWPORT m_pointViewport{};
        D3D12_RECT m_pointScissorRect{};

        u32 m_cascadeSrvIndices[CascadeCount]{
            DescriptorManager::InvalidIndex,
            DescriptorManager::InvalidIndex,
            DescriptorManager::InvalidIndex,
            DescriptorManager::InvalidIndex
        };
        u32 m_pointSrvIndex = DescriptorManager::InvalidIndex;

        D3D12_RESOURCE_STATES m_cascadeStates[CascadeCount]{
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        };
        D3D12_RESOURCE_STATES m_pointState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        DescriptorManager* m_descriptorManager = nullptr;
    };
}
