#pragma once

#include "Graphics/DX12/DX12Common.hpp"
#include <dxcapi.h>
#include <d3dcompiler.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <expected>

namespace RL
{
    using Microsoft::WRL::ComPtr;

    enum class ShaderStage : u8
    {
        Vertex,
        Pixel,
        Compute
    };

    struct ShaderCompileDesc
    {
        std::filesystem::path filePath;
        std::string entryPoint = "main";
        ShaderStage stage = ShaderStage::Vertex;
        std::string shaderModel = "6_5";
        std::vector<std::wstring> extraArguments;
    };

    class ShaderCompiler
    {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;

        [[nodiscard]] std::expected<void, std::string> Initialize();

        [[nodiscard]] std::expected<ComPtr<IDxcBlob>, std::string> CompileShader(
            const ShaderCompileDesc& desc);

        [[nodiscard]] std::expected<ComPtr<IDxcBlob>, std::string> CompileVertexShader(
            const std::filesystem::path& filePath,
            std::string_view entryPoint = "VSMain",
            std::string_view shaderModel = "6_5");

        [[nodiscard]] std::expected<ComPtr<IDxcBlob>, std::string> CompilePixelShader(
            const std::filesystem::path& filePath,
            std::string_view entryPoint = "PSMain",
            std::string_view shaderModel = "6_5");

        [[nodiscard]] std::expected<ComPtr<ID3DBlob>, std::string> CompileShaderModel50(
            const std::filesystem::path& filePath,
            std::string_view entryPoint,
            ShaderStage stage);

    private:
        ComPtr<IDxcUtils> m_utils;
        ComPtr<IDxcCompiler3> m_compiler;
        ComPtr<IDxcIncludeHandler> m_includeHandler;
        bool m_initialized = false;
    };
}
