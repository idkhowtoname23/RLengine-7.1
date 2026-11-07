#pragma once

#include "Core/Types.hpp"
#include <DirectXMath.h>

namespace RL
{
    enum class CameraProjection : u8
    {
        Orthographic = 0,
        Perspective  = 1
    };

    class Camera
    {
    public:
        Camera() noexcept = default;
        ~Camera() noexcept = default;

        Camera(const Camera&) noexcept = default;
        Camera& operator=(const Camera&) noexcept = default;

        Camera(Camera&&) noexcept = default;
        Camera& operator=(Camera&&) noexcept = default;

        void SetPosition(const DirectX::XMFLOAT3& pos) noexcept { m_position = pos; }
        void SetPosition(f32 x, f32 y, f32 z) noexcept { m_position = { x, y, z }; }
        [[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const noexcept { return m_position; }
        void Move(const DirectX::XMFLOAT3& delta) noexcept;
        void Move(f32 dx, f32 dy, f32 dz) noexcept;

        void SetRotation(const DirectX::XMFLOAT3& rot) noexcept { m_rotation = rot; }
        void SetRotation(f32 pitch, f32 yaw, f32 roll) noexcept { m_rotation = { pitch, yaw, roll }; }
        [[nodiscard]] const DirectX::XMFLOAT3& GetRotation() const noexcept { return m_rotation; }

        void SetZoom(f32 zoom) noexcept { m_zoom = (zoom > 0.0001f) ? zoom : 0.0001f; }
        [[nodiscard]] f32 GetZoom() const noexcept { return m_zoom; }

        void SetFOV(f32 fovRadians) noexcept { m_fov = fovRadians; }
        [[nodiscard]] f32 GetFOV() const noexcept { return m_fov; }

        void SetAspectRatio(f32 aspect) noexcept { m_aspectRatio = (aspect > 0.0f) ? aspect : 1.0f; }
        [[nodiscard]] f32 GetAspectRatio() const noexcept { return m_aspectRatio; }

        void SetNearZ(f32 nearZ) noexcept { m_nearZ = nearZ; }
        [[nodiscard]] f32 GetNearZ() const noexcept { return m_nearZ; }

        void SetFarZ(f32 farZ) noexcept { m_farZ = farZ; }
        [[nodiscard]] f32 GetFarZ() const noexcept { return m_farZ; }

        void SetOrthoSize(f32 orthoSize) noexcept { m_orthoSize = orthoSize; }
        [[nodiscard]] f32 GetOrthoSize() const noexcept { return m_orthoSize; }

        void SetProjectionType(CameraProjection type) noexcept { m_projectionType = type; }
        [[nodiscard]] CameraProjection GetProjectionType() const noexcept { return m_projectionType; }

        [[nodiscard]] DirectX::XMMATRIX CalculateViewMatrix() const noexcept;
        [[nodiscard]] DirectX::XMMATRIX CalculateProjectionMatrix() const noexcept;

        [[nodiscard]] DirectX::XMMATRIX GetViewProjectionMatrix() const noexcept;

        [[nodiscard]] DirectX::XMFLOAT3 GetForward() const noexcept
        {
            const auto R = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
            DirectX::XMFLOAT3 fwd;
            DirectX::XMStoreFloat3(&fwd, DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), R));
            return fwd;
        }

        [[nodiscard]] DirectX::XMFLOAT3 GetUp() const noexcept
        {
            const auto R = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
            DirectX::XMFLOAT3 up;
            DirectX::XMStoreFloat3(&up, DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), R));
            return up;
        }

        [[nodiscard]] DirectX::XMFLOAT3 ScreenToWorld2D(
            f32 screenX,
            f32 screenY,
            f32 screenWidth,
            f32 screenHeight) const noexcept;

    private:
        DirectX::XMFLOAT3 m_position{ 0.0f, 0.0f, -10.0f };
        DirectX::XMFLOAT3 m_rotation{ 0.0f, 0.0f, 0.0f };

        f32 m_zoom = 1.0f;
        f32 m_fov = DirectX::XM_PIDIV4;
        f32 m_aspectRatio = 16.0f / 9.0f;
        f32 m_nearZ = 0.1f;
        f32 m_farZ = 1000.0f;
        f32 m_orthoSize = 2.0f;

        CameraProjection m_projectionType = CameraProjection::Orthographic;
    };
}
