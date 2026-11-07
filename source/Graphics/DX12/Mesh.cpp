#include "Graphics/DX12/Mesh.hpp"
#include "Core/Logger.hpp"

#include <cmath>
#include <numbers>

namespace RL
{
    std::expected<std::shared_ptr<Mesh>, std::string> Mesh::CreateFromData(
        ID3D12Device* device,
        std::span<const MeshVertex> vertices,
        std::span<const u32> indices,
        std::string_view name,
        ID3D12CommandQueue* commandQueue)
    {
        if (!device)
        {
            return std::unexpected("Device is null when creating Mesh.");
        }
        if (vertices.empty())
        {
            return std::unexpected("Vertices array is empty when creating Mesh.");
        }

        const u32 maxVertIndex = static_cast<u32>(vertices.size());
        for (u32 idx : indices)
        {
            if (idx >= maxVertIndex)
            {
                return std::unexpected(std::format("Mesh index {} exceeds vertex count {}.", idx, maxVertIndex));
            }
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->m_name = std::string(name);
        mesh->m_vertexCount = static_cast<u32>(vertices.size());
        mesh->m_indexCount = static_cast<u32>(indices.size());

        auto vbRes = mesh->m_vertexBuffer.Initialize(
            device,
            vertices.size_bytes(),
            BufferType::Vertex,
            D3D12_HEAP_TYPE_DEFAULT);
        if (!vbRes)
        {
            return std::unexpected("Failed to allocate default heap vertex buffer for mesh: " + vbRes.error());
        }

        if (!indices.empty())
        {
            auto ibRes = mesh->m_indexBuffer.Initialize(
                device,
                indices.size_bytes(),
                BufferType::Index,
                D3D12_HEAP_TYPE_DEFAULT);
            if (!ibRes)
            {
                return std::unexpected("Failed to allocate default heap index buffer for mesh: " + ibRes.error());
            }
        }

        GPUBuffer stagingVb;
        auto svbRes = stagingVb.Initialize(device, vertices.size_bytes(), BufferType::Generic, D3D12_HEAP_TYPE_UPLOAD);
        if (!svbRes)
        {
            return std::unexpected("Failed to allocate staging vertex buffer: " + svbRes.error());
        }
        stagingVb.UpdateData(vertices);

        GPUBuffer stagingIb;
        if (!indices.empty())
        {
            auto sibRes = stagingIb.Initialize(device, indices.size_bytes(), BufferType::Generic, D3D12_HEAP_TYPE_UPLOAD);
            if (!sibRes)
            {
                return std::unexpected("Failed to allocate staging index buffer: " + sibRes.error());
            }
            stagingIb.UpdateData(indices);
        }

        ComPtr<ID3D12CommandQueue> queue = commandQueue;
        if (!queue)
        {
            D3D12_COMMAND_QUEUE_DESC qDesc{};
            qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            qDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            qDesc.NodeMask = 0;
            RL_CHECK_HR(device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&queue)));
        }

        ComPtr<ID3D12CommandAllocator> cmdAlloc;
        RL_CHECK_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        RL_CHECK_HR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList)));

        D3D12_RESOURCE_BARRIER preBarriers[2]{};
        preBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        preBarriers[0].Transition.pResource = mesh->m_vertexBuffer.GetResource();
        preBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        preBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        preBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        u32 numBarriers = 1;
        if (!indices.empty())
        {
            preBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preBarriers[1].Transition.pResource = mesh->m_indexBuffer.GetResource();
            preBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            preBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            preBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            numBarriers = 2;
        }

        cmdList->ResourceBarrier(numBarriers, preBarriers);

        cmdList->CopyBufferRegion(mesh->m_vertexBuffer.GetResource(), 0, stagingVb.GetResource(), 0, vertices.size_bytes());

        if (!indices.empty())
        {
            cmdList->CopyBufferRegion(mesh->m_indexBuffer.GetResource(), 0, stagingIb.GetResource(), 0, indices.size_bytes());
        }

        D3D12_RESOURCE_BARRIER postBarriers[2]{};
        postBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        postBarriers[0].Transition.pResource = mesh->m_vertexBuffer.GetResource();
        postBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        postBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        postBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        if (!indices.empty())
        {
            postBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postBarriers[1].Transition.pResource = mesh->m_indexBuffer.GetResource();
            postBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            postBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            postBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }

        cmdList->ResourceBarrier(numBarriers, postBarriers);

        RL_CHECK_HR(cmdList->Close());

        ID3D12CommandList* ppCommandLists[] = { cmdList.Get() };
        queue->ExecuteCommandLists(1, ppCommandLists);

        ComPtr<ID3D12Fence> fence;
        RL_CHECK_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HANDLE fenceEvent = CreateEventExW(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        RL_CHECK_HR(queue->Signal(fence.Get(), 1));
        if (fence->GetCompletedValue() < 1)
        {
            RL_CHECK_HR(fence->SetEventOnCompletion(1, fenceEvent));
            WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
        }
        CloseHandle(fenceEvent);

        mesh->m_vbView.BufferLocation = mesh->m_vertexBuffer.GetGPUVirtualAddress();
        mesh->m_vbView.SizeInBytes = static_cast<UINT>(vertices.size_bytes());
        mesh->m_vbView.StrideInBytes = sizeof(MeshVertex);

        if (!indices.empty())
        {
            mesh->m_ibView.BufferLocation = mesh->m_indexBuffer.GetGPUVirtualAddress();
            mesh->m_ibView.SizeInBytes = static_cast<UINT>(indices.size_bytes());
            mesh->m_ibView.Format = DXGI_FORMAT_R32_UINT;
        }

        return mesh;
    }

    std::expected<std::shared_ptr<Mesh>, std::string> Mesh::CreateCube(
        ID3D12Device* device,
        f32 size,
        ID3D12CommandQueue* commandQueue)
    {
        const f32 h = size * 0.5f;

        const MeshVertex vertices[24] = {
            { { -h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { {  h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h, -h,  h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h,  h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h, -h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h, -h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { {  h, -h, -h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h,  h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
        };

        const u32 indices[36] = {
             0,  1,  2,  0,  2,  3,
             4,  5,  6,  4,  6,  7,
             8,  9, 10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };

        return CreateFromData(device, vertices, indices, "Cube", commandQueue);
    }

    std::expected<std::shared_ptr<Mesh>, std::string> Mesh::CreateSphere(
        ID3D12Device* device,
        f32 radius,
        u32 slices,
        u32 stacks,
        ID3D12CommandQueue* commandQueue)
    {
        std::vector<MeshVertex> vertices;
        std::vector<u32> indices;

        vertices.reserve((stacks + 1) * (slices + 1));
        indices.reserve(stacks * slices * 6);

        const f32 pi = std::numbers::pi_v<f32>;

        for (u32 stack = 0; stack <= stacks; ++stack)
        {
            const f32 phi = stack * pi / stacks;
            const f32 sinPhi = std::sin(phi);
            const f32 cosPhi = std::cos(phi);

            for (u32 slice = 0; slice <= slices; ++slice)
            {
                const f32 theta = slice * 2.0f * pi / slices;
                const f32 sinTheta = std::sin(theta);
                const f32 cosTheta = std::cos(theta);

                DirectX::XMFLOAT3 normal{ sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };
                DirectX::XMFLOAT3 pos{ normal.x * radius, normal.y * radius, normal.z * radius };
                DirectX::XMFLOAT2 uv{ static_cast<f32>(slice) / slices, static_cast<f32>(stack) / stacks };

                vertices.push_back({ pos, normal, uv, { 1.0f, 1.0f, 1.0f, 1.0f } });
            }
        }

        for (u32 stack = 0; stack < stacks; ++stack)
        {
            for (u32 slice = 0; slice < slices; ++slice)
            {
                const u32 first = stack * (slices + 1) + slice;
                const u32 second = first + slices + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        return CreateFromData(device, vertices, indices, "Sphere", commandQueue);
    }

    std::expected<std::shared_ptr<Mesh>, std::string> Mesh::CreatePlane(
        ID3D12Device* device,
        f32 width,
        f32 depth,
        u32 subdivisions,
        ID3D12CommandQueue* commandQueue)
    {
        std::vector<MeshVertex> vertices;
        std::vector<u32> indices;

        const u32 countX = subdivisions + 1;
        const u32 countZ = subdivisions + 1;
        const f32 halfW = width * 0.5f;
        const f32 halfD = depth * 0.5f;
        const f32 dx = width / subdivisions;
        const f32 dz = depth / subdivisions;

        vertices.reserve(countX * countZ);
        indices.reserve(subdivisions * subdivisions * 6);

        for (u32 z = 0; z < countZ; ++z)
        {
            const f32 posZ = -halfD + z * dz;
            const f32 v = static_cast<f32>(z) / subdivisions;

            for (u32 x = 0; x < countX; ++x)
            {
                const f32 posX = -halfW + x * dx;
                const f32 u = static_cast<f32>(x) / subdivisions;

                vertices.push_back({
                    { posX, 0.0f, posZ },
                    { 0.0f, 1.0f, 0.0f },
                    { u, v },
                    { 1.0f, 1.0f, 1.0f, 1.0f }
                });
            }
        }

        for (u32 z = 0; z < subdivisions; ++z)
        {
            for (u32 x = 0; x < subdivisions; ++x)
            {
                const u32 row1 = z * countX;
                const u32 row2 = (z + 1) * countX;

                indices.push_back(row1 + x);
                indices.push_back(row2 + x);
                indices.push_back(row1 + x + 1);

                indices.push_back(row1 + x + 1);
                indices.push_back(row2 + x);
                indices.push_back(row2 + x + 1);
            }
        }

        return CreateFromData(device, vertices, indices, "Plane", commandQueue);
    }

    std::expected<std::shared_ptr<Mesh>, std::string> Mesh::CreateGrid(
        ID3D12Device* device,
        f32 size,
        u32 lines,
        ID3D12CommandQueue* commandQueue)
    {
        std::vector<MeshVertex> vertices;
        std::vector<u32> indices;

        const f32 halfSize = size * 0.5f;
        const f32 step = size / lines;

        vertices.reserve((lines + 1) * 4);
        indices.reserve((lines + 1) * 4);

        u32 idx = 0;
        for (u32 i = 0; i <= lines; ++i)
        {
            const f32 pos = -halfSize + i * step;
            const bool isCenter = (std::abs(pos) < 0.001f);
            const i32 gridStepIdx = static_cast<i32>(std::round(pos / step));
            const bool isMajor = (std::abs(gridStepIdx) % 10 == 0);

            DirectX::XMFLOAT4 colorZ;
            if (isCenter)
            {
                colorZ = DirectX::XMFLOAT4{ 0.2f, 0.5f, 1.0f, 1.0f };
            }
            else if (isMajor)
            {
                colorZ = DirectX::XMFLOAT4{ 0.55f, 0.58f, 0.62f, 0.85f };
            }
            else
            {
                colorZ = DirectX::XMFLOAT4{ 0.25f, 0.26f, 0.28f, 0.45f };
            }

            vertices.push_back({ { pos, 0.0f, -halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, colorZ });
            vertices.push_back({ { pos, 0.0f,  halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, colorZ });
            indices.push_back(idx++);
            indices.push_back(idx++);

            DirectX::XMFLOAT4 colorX;
            if (isCenter)
            {
                colorX = DirectX::XMFLOAT4{ 1.0f, 0.25f, 0.25f, 1.0f };
            }
            else if (isMajor)
            {
                colorX = DirectX::XMFLOAT4{ 0.55f, 0.58f, 0.62f, 0.85f };
            }
            else
            {
                colorX = DirectX::XMFLOAT4{ 0.25f, 0.26f, 0.28f, 0.45f };
            }

            vertices.push_back({ { -halfSize, 0.0f, pos }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, colorX });
            vertices.push_back({ {  halfSize, 0.0f, pos }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, colorX });
            indices.push_back(idx++);
            indices.push_back(idx++);
        }

        return CreateFromData(device, vertices, indices, "Grid", commandQueue);
    }

    void Mesh::Bind(ID3D12GraphicsCommandList* cmdList) const noexcept
    {
        if (!cmdList || m_vertexCount == 0) return;
        cmdList->IASetVertexBuffers(0, 1, &m_vbView);
        if (m_indexCount > 0)
        {
            cmdList->IASetIndexBuffer(&m_ibView);
        }
    }

    void Mesh::Draw(ID3D12GraphicsCommandList* cmdList) const noexcept
    {
        if (!cmdList) return;
        if (m_indexCount > 0)
        {
            cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
        }
        else
        {
            cmdList->DrawInstanced(m_vertexCount, 1, 0, 0);
        }
    }
}
