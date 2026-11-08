cmake_minimum_required(VERSION 3.28)

project(RLengine7_1 VERSION 7.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(LUA_SOURCES
    third_party/lua/src/lapi.c
    third_party/lua/src/lauxlib.c
    third_party/lua/src/lbaselib.c
    third_party/lua/src/lcode.c
    third_party/lua/src/lcorolib.c
    third_party/lua/src/lctype.c
    third_party/lua/src/ldblib.c
    third_party/lua/src/ldebug.c
    third_party/lua/src/ldo.c
    third_party/lua/src/ldump.c
    third_party/lua/src/lfunc.c
    third_party/lua/src/lgc.c
    third_party/lua/src/linit.c
    third_party/lua/src/liolib.c
    third_party/lua/src/llex.c
    third_party/lua/src/lmathlib.c
    third_party/lua/src/lmem.c
    third_party/lua/src/loadlib.c
    third_party/lua/src/lobject.c
    third_party/lua/src/lopcodes.c
    third_party/lua/src/loslib.c
    third_party/lua/src/lparser.c
    third_party/lua/src/lstate.c
    third_party/lua/src/lstring.c
    third_party/lua/src/lstrlib.c
    third_party/lua/src/ltable.c
    third_party/lua/src/ltablib.c
    third_party/lua/src/ltm.c
    third_party/lua/src/lundump.c
    third_party/lua/src/lutf8lib.c
    third_party/lua/src/lvm.c
    third_party/lua/src/lzio.c
)

add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/src"
)
if(MSVC)
    target_compile_options(lua PRIVATE /W3)
    target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/backends/imgui_impl_win32.cpp
    vendor/imgui/backends/imgui_impl_dx12.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends"
)
if(MSVC)
    target_compile_options(imgui PRIVATE /W3)
    target_compile_definitions(imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set(CORE_SOURCES
    source/Core/Types.hpp
    source/Core/Logger.hpp
    source/Core/HResult.hpp
    source/Core/Timer.hpp
    source/Core/StringID.hpp
    source/Core/UUID.hpp
    source/Core/ServiceLocator.hpp
    source/Core/Events/CommonEvents.hpp
    source/Core/Events/EventBus.hpp
    source/Core/Memory/Allocators.hpp
    source/Core/Memory/MemoryStats.hpp
    source/Core/Reflect/TypeInfo.hpp
)

set(PLATFORM_SOURCES
    source/Platform/Window.hpp
    source/Platform/Window.cpp
    source/Platform/Input.hpp
    source/Platform/Input.cpp
    source/Platform/CrashHandler.hpp
    source/Platform/CrashHandler.cpp
    source/Platform/FileSystem/FileWatcher.hpp
    source/Platform/FileSystem/VFS.hpp
    source/Platform/RLengine.rc
)

set(ASSETS_SOURCES
    source/Assets/AssetHandle.hpp
    source/Assets/AssetManager.hpp
)

set(GRAPHICS_SOURCES
    source/Graphics/RHI/RHICommon.hpp
    source/Graphics/RHI/RHIInterfaces.hpp
    source/Graphics/RHI/GpuCapabilities.hpp
    source/Graphics/DX11/DX11Device.hpp
    source/Graphics/DX11/DX11Device.cpp
    source/Graphics/DX12/DX12Common.hpp
    source/Graphics/DX12/DX12Device.hpp
    source/Graphics/DX12/DX12Device.cpp
    source/Graphics/DX12/DX12Buffer.hpp
    source/Graphics/DX12/DX12Buffer.cpp
    source/Graphics/DX12/Vertex.hpp
    source/Graphics/DX12/Mesh.hpp
    source/Graphics/DX12/Mesh.cpp
    source/Graphics/DX12/GLTFLoader.hpp
    source/Graphics/DX12/GLTFLoader.cpp
    source/Graphics/DX12/ShaderCompiler.hpp
    source/Graphics/DX12/ShaderCompiler.cpp
    source/Graphics/DX12/Pipeline.hpp
    source/Graphics/DX12/Pipeline.cpp
    source/Graphics/DX12/DescriptorManager.hpp
    source/Graphics/DX12/DescriptorManager.cpp
    source/Graphics/DX12/ShadowMap.hpp
    source/Graphics/DX12/ShadowMap.cpp
    source/Graphics/DX12/DX12Texture.hpp
    source/Graphics/DX12/DX12Texture.cpp
    source/Graphics/DX12/TextureManager.hpp
    source/Graphics/DX12/TextureManager.cpp
    source/Graphics/DX12/ParticleSystem.hpp
    source/Graphics/DX12/ParticleSystem.cpp
)

set(SCENE_SOURCES
    source/Scene/Entity.hpp
    source/Scene/Components.hpp
    source/Scene/Camera.hpp
    source/Scene/Camera.cpp
    source/Scene/Scene.hpp
    source/Scene/Scene.cpp
    source/Scene/SceneSerializer.hpp
    source/Scene/SceneSerializer.cpp
    source/Scene/StressTest.hpp
    source/Scene/StressTest.cpp
    source/Scene/Systems/RenderSystem.hpp
    source/Scene/Systems/RenderSystem.cpp
    source/Scene/Systems/PhysicsSystem.hpp
    source/Scene/Systems/PhysicsSystem.cpp
)

set(SCRIPTING_SOURCES
    source/Scripting/ScriptEngine.hpp
    source/Scripting/ScriptEngine.cpp
    source/Scripting/LuaEntityBridge.hpp
    source/Scripting/LuaEntityBridge.cpp
    source/Scripting/LuaInputBridge.hpp
    source/Scripting/LuaInputBridge.cpp
)

set(EDITOR_SOURCES
    source/Editor/EditorLayer.hpp
    source/Editor/EditorLayer.cpp
    source/Editor/LuaConsole.hpp
    source/Editor/LuaConsole.cpp
    source/Editor/SceneBuilder.hpp
    source/Editor/SceneBuilder.cpp
    source/Editor/Gizmo/GizmoSystem.hpp
    source/Editor/History/CommandHistory.hpp
    source/Editor/Panels/ContentBrowserPanel.hpp
    source/Editor/Panels/InspectorPanel.hpp
    source/Editor/Panels/SceneHierarchyPanel.hpp
    source/Editor/Panels/StatsPanel.hpp
    source/Editor/UI/Panel.hpp
    source/Editor/UI/PanelManager.hpp
    source/Editor/UI/Theme.hpp
)

set(AUDIO_SOURCES
    source/Audio/AudioEngine.hpp
    source/Audio/AudioEngine.cpp
    source/Audio/AudioSystem.hpp
    source/Audio/AudioSystem.cpp
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${ASSETS_SOURCES}
    ${GRAPHICS_SOURCES}
    ${SCENE_SOURCES}
    ${AUDIO_SOURCES}
    ${SCRIPTING_SOURCES}
    ${EDITOR_SOURCES}
    source/main.cpp
)

add_executable(RLengine7.1 ${ALL_SOURCES})
set_target_properties(RLengine7.1 PROPERTIES OUTPUT_NAME "RLengine7.1")

target_include_directories(RLengine7.1 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cgltf"
)

if(MSVC)
    target_compile_options(RLengine7.1 PRIVATE
        /W4
        /permissive-
        /utf-8
        /Zc:__cplusplus
        /Zc:preprocessor
        /EHsc
        /MP
        /wd4324
        /wd4100
        /wd4189
        /wd4244
        /wd4267
        /wd4305
        /wd4456
        /wd4458
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Ot>
        $<$<CONFIG:Release>:/fp:fast>
    )

    target_compile_definitions(RLengine7.1 PRIVATE
        _UNICODE
        UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
endif()

target_link_libraries(RLengine7.1 PRIVATE
    d3d11
    d3d12
    dxgi
    dxguid
    d3dcompiler
    dxcompiler
    dbghelp
    lua
    imgui
)

source_group("Core" FILES ${CORE_SOURCES})
source_group("Platform" FILES ${PLATFORM_SOURCES})
source_group("Assets" FILES ${ASSETS_SOURCES})
source_group("Graphics\\RHI" FILES source/Graphics/RHI/RHICommon.hpp source/Graphics/RHI/GpuCapabilities.hpp)
source_group("Graphics\\DX11" FILES source/Graphics/DX11/DX11Device.hpp source/Graphics/DX11/DX11Device.cpp)
source_group("Graphics\\DX12" FILES ${GRAPHICS_SOURCES})
source_group("Scene" FILES ${SCENE_SOURCES})
source_group("Scripting" FILES ${SCRIPTING_SOURCES})
source_group("Editor" FILES ${EDITOR_SOURCES})

file(GLOB WINDOWS_KITS_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
list(SORT WINDOWS_KITS_DIRS COMPARE NATURAL ORDER DESCENDING)
set(WINDOWS_KITS_BIN "")
foreach(KIT_DIR ${WINDOWS_KITS_DIRS})
    if(EXISTS "${KIT_DIR}/x64/dxcompiler.dll")
        set(WINDOWS_KITS_BIN "${KIT_DIR}/x64")
        break()
    endif()
endforeach()

if(EXISTS "${WINDOWS_KITS_BIN}/dxcompiler.dll")
    add_custom_command(TARGET RLengine7.1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINDOWS_KITS_BIN}/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RLengine7.1>/dxcompiler.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINDOWS_KITS_BIN}/dxil.dll"
            "$<TARGET_FILE_DIR:RLengine7.1>/dxil.dll"
        COMMENT "Deploying DXC runtime DLLs to output directory"
    )
endif()

add_custom_command(TARGET RLengine7.1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:RLengine7.1>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:RLengine7.1>/scripts"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RLengine7.1>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:RLengine7.1>/assets"
    COMMENT "Deploying shaders, scripts, and assets to output directory"
)
