#include "Core/Types.hpp"
#include "Core/Logger.hpp"
#include "Core/Timer.hpp"
#include "Platform/CrashHandler.hpp"
#include "Platform/Window.hpp"
#include "Platform/Input.hpp"
#include "Graphics/DX12/DX12Device.hpp"
#include "Graphics/DX11/DX11Device.hpp"
#include "Graphics/DX12/Vertex.hpp"
#include "Graphics/DX12/ShaderCompiler.hpp"
#include "Graphics/DX12/Pipeline.hpp"
#include "Graphics/DX12/DescriptorManager.hpp"
#include "Graphics/DX12/TextureManager.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneSerializer.hpp"
#include "Scene/Camera.hpp"
#include "Scene/Systems/RenderSystem.hpp"
#include "Scripting/ScriptEngine.hpp"
#include "Scripting/LuaEntityBridge.hpp"
#include "Scene/StressTest.hpp"
#include "Editor/EditorLayer.hpp"
#include "Audio/AudioEngine.hpp"
#include "Audio/AudioSystem.hpp"
#include "Platform/FileSystem/VFS.hpp"
#include "Core/Events/EventBus.hpp"
#include "Core/Events/CommonEvents.hpp"

#include <filesystem>
#include <format>
#include <string_view>
#include <cstdlib>

namespace RL
{
    [[nodiscard]] inline std::filesystem::path ResolveAssetPath(const std::filesystem::path& relativePath)
    {
        const std::string relStr = relativePath.string();
        if (relStr.find("://") != std::string::npos)
        {
            const auto resolved = VFS::Resolve(relStr);
            if (resolved) return *resolved;
        }

        const auto vfsResolved = VFS::Resolve("assets://" + relStr);
        if (vfsResolved) return *vfsResolved;

        if (std::filesystem::exists(relativePath))
        {
            return relativePath;
        }

        wchar_t exePathBuf[MAX_PATH];
        GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
        const std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
        if (std::filesystem::exists(exeDir / relativePath))
        {
            return exeDir / relativePath;
        }

        const std::filesystem::path candidate = (exeDir / ".." / ".." / ".." / relativePath).lexically_normal();
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }

        return relativePath;
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    CrashHandler::Initialize();

    using namespace RL;

    Logger::SetMinLogLevel(LogLevel::Trace);
    RL_LOG_INFO("=================================================");
    RL_LOG_INFO("     RLengine 7.1 - Next-Generation Engine      ");
    RL_LOG_INFO("  1,000,000 Entities GPU Instanced Benchmark    ");
    RL_LOG_INFO("=================================================");

    bool smokeTest = false;
    u32 maxTestFrames = 15;
    bool stressTestMode = false;
    u32 stressEntityCount = 5000;
    bool useDX11 = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == "--dx11")
        {
            useDX11 = true;
        }
        else if (std::string_view(argv[i]) == "--smoke-test")
        {
            smokeTest = true;
            RL_LOG_INFO("Running in automated smoke test mode (will render {} frames).", maxTestFrames);
        }
        else if (std::string_view(argv[i]) == "--stress-test")
        {
            stressTestMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                stressEntityCount = static_cast<u32>(std::atoi(argv[++i]));
                if (stressEntityCount == 0) stressEntityCount = 5000;
            }
            RL_LOG_INFO("Running in stress test mode (will spawn {} entities).", stressEntityCount);
        }
    }

    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    const std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();

    auto findRoot = [&](const std::string& sub) -> std::filesystem::path {
        if (std::filesystem::exists(sub)) return std::filesystem::absolute(sub);
        if (std::filesystem::exists(exeDir / sub)) return exeDir / sub;
        if (std::filesystem::exists(exeDir / ".." / ".." / ".." / sub)) return (exeDir / ".." / ".." / ".." / sub).lexically_normal();
        return std::filesystem::current_path() / sub;
    };

    VFS::Mount("assets://", findRoot("assets"));
    VFS::Mount("shaders://", findRoot("shaders"));
    VFS::Mount("scripts://", findRoot("scripts"));
    VFS::PrintMounts();

    WindowDesc windowDesc{
        .title = L"RLengine 7.1 [DirectX 12 + Lua 5.4 | ECS Scene]",
        .width = 1600,
        .height = 900,
        .resizable = true
    };

    Window window;
    auto windowInitRes = window.Initialize(windowDesc);
    if (!windowInitRes)
    {
        RL_LOG_FATAL("Window initialization failed: {}", windowInitRes.error());
        return -1;
    }

    DX12Device device;
    if (!useDX11)
    {
        auto deviceInitRes = device.Initialize(window.GetHWND(), window.GetWidth(), window.GetHeight());
        if (!deviceInitRes)
        {
            RL_LOG_WARN("DirectX 12 initialization failed ({}); automatically falling back to DirectX 11...", deviceInitRes.error());
            useDX11 = true;
        }
    }

    if (useDX11)
    {
        RL_LOG_INFO("Operating in DirectX 11 Fallback / Legacy mode (GTX 500-700 / Feature Level 11_0)...");
        DX11Device dx11;
        auto d11Res = dx11.Initialize(window.GetHWND(), window.GetWidth(), window.GetHeight());
        if (!d11Res)
        {
            RL_LOG_FATAL("DirectX 11 initialization failed: {}", d11Res.error());
            return -2;
        }

        Scene scene;
        scene.GetCamera().SetProjectionType(CameraProjection::Perspective);
        scene.GetCamera().SetPosition({ 0.0f, 2.0f, -5.0f });
        scene.GetCamera().SetRotation({ 0.2f, 0.0f, 0.0f });
        scene.GetCamera().SetAspectRatio(window.GetAspectRatio());

        window.SetResizeCallback([&dx11, &scene](u32 width, u32 height) {
            EventBus::Publish(WindowResizedEvent{ width, height });
            dx11.Resize(width, height);
            if (height > 0)
            {
                scene.GetCamera().SetAspectRatio(static_cast<f32>(width) / static_cast<f32>(height));
            }
        });

        Entity cube = scene.CreateEntity("DX11_Cube");
        scene.AddComponent<MeshComponent>(cube, "Cube");
        auto& tr = scene.GetComponent<TransformComponent>(cube);
        tr.Position = { 0.0f, 0.0f, 0.0f };

        Entity child = scene.CreateEntity("DX11_ChildCube");
        scene.AddComponent<MeshComponent>(child, "ChildCube");
        auto& childTr = scene.GetComponent<TransformComponent>(child);
        childTr.Position = { 1.5f, 0.5f, 0.0f };
        scene.SetParent(child, cube);

        Timer timer;
        while (true)
        {
            Input::NewFrame();
            EventBus::DispatchDeferred();
            AudioEngine::Get().Update();
            if (!window.ProcessMessages() || window.ShouldClose()) break;

            timer.Tick();
            const f32 dt = timer.GetDeltaTime();

            tr.Rotation.y += dt * 0.75f;
            childTr.Rotation.x += dt * 1.5f;

            if (Input::IsKeyDown(KeyCode::W)) scene.GetCamera().Move(0.0f, 0.0f, 5.0f * dt);
            if (Input::IsKeyDown(KeyCode::S)) scene.GetCamera().Move(0.0f, 0.0f, -5.0f * dt);
            if (Input::IsKeyDown(KeyCode::A)) scene.GetCamera().Move(-5.0f * dt, 0.0f, 0.0f);
            if (Input::IsKeyDown(KeyCode::D)) scene.GetCamera().Move(5.0f * dt, 0.0f, 0.0f);
            if (Input::IsKeyDown(KeyCode::Q)) scene.GetCamera().Move(0.0f, -5.0f * dt, 0.0f);
            if (Input::IsKeyDown(KeyCode::E)) scene.GetCamera().Move(0.0f, 5.0f * dt, 0.0f);

            scene.Update(dt);

            dx11.BeginFrame();
            dx11.ClearRenderTarget(0.04f, 0.06f, 0.09f, 1.0f);
            dx11.RenderScene(scene);
            dx11.EndFrame();
            dx11.Present(true);

            if (smokeTest && timer.GetFrameCount() >= maxTestFrames)
            {
                const usize originalCount = scene.GetAliveEntityCount();
                SceneSerializer::Save(scene, "assets/scenes/smoke_test.json");
                SceneSerializer::Save(scene, "assets/scenes/smoke_test.bin");
                Scene jsonScene;
                SceneSerializer::Load(jsonScene, nullptr, "assets/scenes/smoke_test.json");
                Scene binScene;
                SceneSerializer::Load(binScene, nullptr, "assets/scenes/smoke_test.bin");
                if (jsonScene.GetAliveEntityCount() != originalCount || binScene.GetAliveEntityCount() != originalCount)
                {
                    RL_LOG_FATAL("DirectX 11 smoke test entity count mismatch: original {}, json {}, bin {}",
                        originalCount, jsonScene.GetAliveEntityCount(), binScene.GetAliveEntityCount());
                    return -12;
                }
                RL_LOG_INFO("DirectX 11 Smoke test completed successfully ({} frames rendered, entity count verified: {}).", timer.GetFrameCount(), originalCount);
                break;
            }
        }
        window.SetResizeCallback(nullptr);
        dx11.Shutdown();
        RL_LOG_INFO("DirectX 11 shutdown completed cleanly.");
        return 0;
    }

    D3D12_VIEWPORT viewport{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<f32>(window.GetWidth()),
        .Height = static_cast<f32>(window.GetHeight()),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };

    D3D12_RECT scissorRect{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(window.GetWidth()),
        .bottom = static_cast<LONG>(window.GetHeight())
    };

    Scene scene;
    scene.GetCamera().SetAspectRatio(window.GetAspectRatio());

    window.SetResizeCallback([&device, &viewport, &scissorRect, &scene](u32 width, u32 height) {
        EventBus::Publish(WindowResizedEvent{ width, height });
        device.Resize(width, height);
        viewport.Width = static_cast<f32>(width);
        viewport.Height = static_cast<f32>(height);
        scissorRect.right = static_cast<LONG>(width);
        scissorRect.bottom = static_cast<LONG>(height);
        if (height > 0)
        {
            scene.GetCamera().SetAspectRatio(static_cast<f32>(width) / static_cast<f32>(height));
        }
    });

    ShaderCompiler shaderCompiler;
    auto scInitRes = shaderCompiler.Initialize();
    if (!scInitRes)
    {
        RL_LOG_FATAL("Shader compiler initialization failed: {}", scInitRes.error());
        return -3;
    }

    const auto shaderPath = ResolveAssetPath("shaders/quad.hlsl");
    RL_LOG_INFO("Compiling shaders from '{}'...", shaderPath.string());

    auto vsBlobRes = shaderCompiler.CompileVertexShader(shaderPath, "VSMain", "6_5");
    if (!vsBlobRes)
    {
        RL_LOG_FATAL("Vertex shader compilation failed: {}", vsBlobRes.error());
        return -4;
    }

    auto psBlobRes = shaderCompiler.CompilePixelShader(shaderPath, "PSMain", "6_5");
    if (!psBlobRes)
    {
        RL_LOG_FATAL("Pixel shader compilation failed: {}", psBlobRes.error());
        return -5;
    }

    const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, Color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, UV),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    PipelineConfig quadCfg{};
    quadCfg.TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    quadCfg.CullMode = D3D12_CULL_MODE_NONE;
    quadCfg.FillMode = D3D12_FILL_MODE_SOLID;
    quadCfg.RtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    quadCfg.EnableBlend = true;
    quadCfg.EnableDepth = true;
    quadCfg.DepthWrite = false;
    quadCfg.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    quadCfg.DsvFormat = DXGI_FORMAT_D32_FLOAT;

    Pipeline pipeline;
    auto pipeInitRes = pipeline.Initialize(
        device.GetDevice(),
        vsBlobRes->Get(),
        psBlobRes->Get(),
        inputElements,
        quadCfg
    );
    if (!pipeInitRes)
    {
        RL_LOG_FATAL("Pipeline creation failed: {}", pipeInitRes.error());
        return -6;
    }

    DescriptorManager descriptorManager;
    auto dmInitRes = descriptorManager.Initialize(device.GetDevice(), 1024);
    if (!dmInitRes)
    {
        RL_LOG_FATAL("DescriptorManager initialization failed: {}", dmInitRes.error());
        return -7;
    }

    TextureManager textureManager;
    auto tmInitRes = textureManager.Initialize(device, descriptorManager);
    if (!tmInitRes)
    {
        RL_LOG_FATAL("TextureManager initialization failed: {}", tmInitRes.error());
        return -8;
    }

    RenderSystem renderSystem;
    auto rsInitRes = renderSystem.Initialize(device.GetDevice(), &descriptorManager, &textureManager, &shaderCompiler);
    if (!rsInitRes)
    {
        RL_LOG_FATAL("RenderSystem initialization failed: {}", rsInitRes.error());
        return -9;
    }
    renderSystem.SetQuadPipeline(&pipeline);
    scene.SetRenderSystem(&renderSystem);

    ScriptEngine scriptEngine;
    const auto scriptPath = ResolveAssetPath("scripts/main.lua");
    auto scriptInitRes = scriptEngine.Initialize(scriptPath, &scene, &textureManager, &window);
    if (!scriptInitRes)
    {
        RL_LOG_FATAL("Script engine initialization failed: {}", scriptInitRes.error());
        return -10;
    }

    EditorLayer editorLayer;
    auto editorInitRes = editorLayer.Initialize(
        window.GetHWND(),
        device.GetDevice(),
        NUM_FRAMES,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        &scriptEngine
    );
    if (!editorInitRes)
    {
        RL_LOG_FATAL("EditorLayer initialization failed: {}", editorInitRes.error());
        return -11;
    }

    auto partInitRes = editorLayer.GetSceneBuilder().GetParticleSystem().Initialize(
        device.GetDevice(),
        &descriptorManager,
        &shaderCompiler
    );
    if (!partInitRes)
    {
        RL_LOG_WARN("ParticleSystem initialization: {}", partInitRes.error());
    }
    scene.SetParticleSystem(&editorLayer.GetSceneBuilder().GetParticleSystem());
    scene.SetPhysicsSystem(&editorLayer.GetSceneBuilder().GetPhysicsSystem());

    auto audioInitRes = AudioEngine::Get().Initialize();
    if (!audioInitRes)
    {
        RL_LOG_WARN("AudioEngine initialization: {}", audioInitRes.error());
    }
    AudioSystem audioSystem;
    scene.SetAudioSystem(&audioSystem);

    StressTest stressTest;
    LuaEntityBridge::SetStressTest(&stressTest);
    if (stressTestMode)
    {
        editorLayer.GetSceneBuilder().SetVSyncEnabled(false);
        scene.GetCamera().SetProjectionType(CameraProjection::Perspective);
        scene.GetCamera().SetPosition({ 0.0f, 0.0f, -30.0f });
        scene.GetCamera().SetRotation({ 0.0f, 0.0f, 0.0f });
        scene.GetCamera().SetFOV(DirectX::XMConvertToRadians(45.0f));
        scene.GetCamera().SetAspectRatio(window.GetAspectRatio());
        stressTest.Spawn(scene, stressEntityCount);
    }
    if (smokeTest)
    {
        scene.GetCamera().SetProjectionType(CameraProjection::Perspective);
        Entity cubeEnt = scene.CreateEntity("Cube");
        scene.AddComponent<MeshComponent>(cubeEnt, "Cube");
    }

    Timer& timer = window.GetTimer();
    timer.Reset();

    f32 titleUpdateTimer = 0.0f;
    RL_LOG_INFO("All subsystems initialized successfully. Entering engine main loop...");

    while (true)
    {
        Input::NewFrame();
        EventBus::DispatchDeferred();
        AudioEngine::Get().Update();

        if (!window.ProcessMessages() || window.ShouldClose())
        {
            break;
        }

        timer.Tick();
        const f32 dt = timer.GetDeltaTime();

        if (window.IsMinimized())
        {
            Sleep(10);
            continue;
        }

        if (editorLayer.GetSceneBuilder().IsSimulationRunning())
        {
            scriptEngine.Tick(dt);
            scene.Update(dt);
            stressTest.Update(scene, dt);
        }

        titleUpdateTimer += dt;
        if (titleUpdateTimer >= 0.25f)
        {
            titleUpdateTimer = 0.0f;
            const auto title = std::format(
                "RLengine 7.1 | GPU: {} | {}x{} | {} FPS ({:.2f} ms) | ECS Entities: {} (Rendered: {})",
                device.GetGpuDescription(),
                window.GetWidth(),
                window.GetHeight(),
                timer.GetFPS(),
                timer.GetFrameTimeMS(),
                scene.GetAliveEntityCount(),
                renderSystem.GetLastRenderedCount()
            );
            window.SetTitle(title);
        }

        editorLayer.BeginFrame();
        editorLayer.RenderUI(timer, device, scene, renderSystem.GetLastRenderedCount(), &renderSystem, &textureManager, &scriptEngine);

        device.BeginFrame();

        device.ClearRenderTarget(0.04f, 0.06f, 0.09f, 1.0f);

        ID3D12GraphicsCommandList* cmdList = device.GetCommandList();

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissorRect);

        renderSystem.SetViewport(viewport, scissorRect);
        renderSystem.SetMainRenderTarget(device.GetRTVHandle(), device.GetDSVHandle());

        const u32 currentFrameIndex = device.GetCurrentFrameIndex();
        scene.Render(cmdList, currentFrameIndex);

        editorLayer.EndFrame(cmdList);

        device.EndFrame();

        const bool vsync = editorLayer.GetSceneBuilder().IsVSyncEnabled();
        device.Present(vsync);

        if (smokeTest && timer.GetFrameCount() >= maxTestFrames)
        {
            const usize originalCount = scene.GetAliveEntityCount();
            SceneSerializer::Save(scene, "assets/scenes/smoke_test.json");
            SceneSerializer::Save(scene, "assets/scenes/smoke_test.bin");
            Scene jsonScene;
            SceneSerializer::Load(jsonScene, device.GetDevice(), "assets/scenes/smoke_test.json");
            Scene binScene;
            SceneSerializer::Load(binScene, device.GetDevice(), "assets/scenes/smoke_test.bin");
            if (jsonScene.GetAliveEntityCount() != originalCount || binScene.GetAliveEntityCount() != originalCount)
            {
                RL_LOG_FATAL("Smoke test entity count mismatch: original {}, json {}, bin {}",
                    originalCount, jsonScene.GetAliveEntityCount(), binScene.GetAliveEntityCount());
                return -12;
            }
            RL_LOG_INFO("Smoke test completed successfully ({} frames rendered, JSON & Binary scene serialization verified, entity count verified: {}, rendered count: {}).",
                timer.GetFrameCount(), originalCount, renderSystem.GetLastRenderedCount());
            break;
        }
    }

    RL_LOG_INFO("Exiting engine main loop. Performing clean shutdown...");

    device.WaitForGPU();

    editorLayer.Shutdown();
    AudioEngine::Get().Shutdown();
    renderSystem.Destroy();
    scriptEngine.Shutdown();
    textureManager.Shutdown();
    descriptorManager.Shutdown();
    device.Shutdown();
    window.Shutdown();

    RL_LOG_INFO("RLengine 7.1 shutdown completed successfully.");
    return 0;
}
