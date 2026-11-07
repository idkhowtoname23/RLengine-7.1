#include "Audio/AudioSystem.hpp"
#include "Audio/AudioEngine.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"

namespace RL
{
    void AudioSystem::Update(Scene& scene, f32)
    {
        AudioEngine::Get().Update();

        const auto& cam = scene.GetCamera();
        const auto camPos = cam.GetPosition();
        const auto camFwd = cam.GetForward();
        const auto camUp = cam.GetUp();

        AudioEngine::Get().SetListenerPosition(camPos.x, camPos.y, camPos.z);
        AudioEngine::Get().SetListenerDirection(camFwd.x, camFwd.y, camFwd.z, camUp.x, camUp.y, camUp.z);

        const auto* audioPool = scene.GetPool<AudioSourceComponent>();
        if (!audioPool) return;

        const auto entities = audioPool->GetEntities();
        for (usize i = 0; i < entities.size(); ++i)
        {
            const u32 entityIdx = entities[i];
            Entity entity(entityIdx, 1);
            auto* src = scene.TryGetComponent<AudioSourceComponent>(entity);
            if (!src) continue;

            if (src->PlayOnStart && !src->IsPlaying && !src->SoundPath.empty())
            {
                if (src->Is3D)
                {
                    const auto* tr = scene.TryGetComponent<TransformComponent>(entity);
                    const auto pos = tr ? tr->Position : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
                    AudioEngine::Get().PlaySound3D(src->SoundPath, pos.x, pos.y, pos.z, src->Volume, src->MinDistance, src->MaxDistance);
                }
                else
                {
                    AudioEngine::Get().PlaySound(src->SoundPath, src->Volume);
                }
                src->IsPlaying = true;
            }
        }
    }
}
