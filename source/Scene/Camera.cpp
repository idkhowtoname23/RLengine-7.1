#include "Scene/Camera.hpp"

namespace RL
{
    void Camera::Move(const DirectX::XMFLOAT3& delta) noexcept
    {
        m_position.x += delta.x;
        m_position.y += delta.y;
        m_position.z += delta.z;
    }

    void Camera::Move(f32 dx, f32 dy, f32 dz) noexcept
    {
        m_position.x += dx;
        m_position.y += dy;
        m_position.z += dz;
    }

    DirectX::XMMATRIX Camera::CalculateViewMatrix() const noexcept
    {
        using namespace DirectX;
        const XMVECTOR eye = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f);

        const XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(m_rotation.x),
            XMConvertToRadians(m_rotation.y),
            XMConvertToRadians(m_rotation.z)
        );

        const XMVECTOR defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR defaultUp      = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        const XMVECTOR forward = XMVector3TransformNormal(defaultForward, rotMatrix);
        const XMVECTOR up      = XMVector3TransformNormal(defaultUp, rotMatrix);
        const XMVECTOR target  = XMVectorAdd(eye, forward);

        return XMMatrixLookAtLH(eye, target, up);
    }

    DirectX::XMMATRIX Camera::CalculateProjectionMatrix() const noexcept
    {
        using namespace DirectX;
        if (m_projectionType == CameraProjection::Perspective)
        {
            return XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ);
        }
        else
        {
            const f32 safeZoom   = (m_zoom > 0.0001f) ? m_zoom : 0.0001f;
            const f32 viewHeight = m_orthoSize / safeZoom;
            const f32 viewWidth  = viewHeight * m_aspectRatio;
            return XMMatrixOrthographicLH(viewWidth, viewHeight, m_nearZ, m_farZ);
        }
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const noexcept
    {
        using namespace DirectX;
        const XMMATRIX view = CalculateViewMatrix();
        const XMMATRIX proj = CalculateProjectionMatrix();
        return XMMatrixMultiply(view, proj);
    }

    DirectX::XMFLOAT3 Camera::ScreenToWorld2D(
        f32 screenX,
        f32 screenY,
        f32 screenWidth,
        f32 screenHeight) const noexcept
    {
        if (screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            return m_position;
        }

        const f32 ndcX = (2.0f * screenX / screenWidth) - 1.0f;
        const f32 ndcY = 1.0f - (2.0f * screenY / screenHeight);

        const f32 safeZoom   = (m_zoom > 0.0001f) ? m_zoom : 0.0001f;
        const f32 viewHeight = m_orthoSize / safeZoom;
        const f32 viewWidth  = viewHeight * m_aspectRatio;

        const f32 worldX = m_position.x + ndcX * (viewWidth * 0.5f);
        const f32 worldY = m_position.y + ndcY * (viewHeight * 0.5f);
        const f32 worldZ = 0.0f;

        return { worldX, worldY, worldZ };
    }
}
