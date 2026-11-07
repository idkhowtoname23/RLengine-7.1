#include "Graphics/DX12/ShaderCompiler.hpp"
#include "Core/HResult.hpp"

#include <format>

namespace RL
{
    namespace
    {
        std::wstring ToWideString(std::string_view str)
        {
            if (str.empty())
            {
                return {};
            }

            const int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            if (len <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<usize>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), len);
            return result;
        }
    }

    std::expected<void, std::string> ShaderCompiler::Initialize()
    {
        if (m_initialized)
        {
            return {};
        }

        RL_CHECK_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));
        RL_CHECK_HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler)));
        RL_CHECK_HR(m_utils->CreateDefaultIncludeHandler(&m_includeHandler));

        m_initialized = true;
        RL_LOG_INFO("Shader Compiler (DXC SM 6.x + D3DCompile SM 5.0) initialized successfully.");
        return {};
    }

    std::expected<ComPtr<IDxcBlob>, std::string> ShaderCompiler::CompileShader(const ShaderCompileDesc& desc)
    {
        if (!m_initialized)
        {
            auto initRes = Initialize();
            if (!initRes)
            {
                return std::unexpected(initRes.error());
            }
        }

        if (!std::filesystem::exists(desc.filePath))
        {
            return std::unexpected(std::format("Shader source file does not exist: '{}'", desc.filePath.string()));
        }

        ComPtr<IDxcBlobEncoding> sourceBlob;
        const std::wstring widePath = desc.filePath.wstring();
        const HRESULT loadHr = m_utils->LoadFile(widePath.c_str(), nullptr, &sourceBlob);
        if (FAILED(loadHr))
        {
            return std::unexpected(std::format("Failed to load shader file '{}'. Error: 0x{:08X}",
                desc.filePath.string(), static_cast<u32>(loadHr)));
        }

        DxcBuffer sourceBuffer{};
        sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
        sourceBuffer.Size = sourceBlob->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        std::vector<std::wstring> arguments;
        arguments.push_back(desc.filePath.filename().wstring());

        arguments.push_back(L"-E");
        arguments.push_back(ToWideString(desc.entryPoint));

        std::wstring targetProfile;
        switch (desc.stage)
        {
            case ShaderStage::Vertex:
                targetProfile = L"vs_" + ToWideString(desc.shaderModel);
                break;
            case ShaderStage::Pixel:
                targetProfile = L"ps_" + ToWideString(desc.shaderModel);
                break;
            case ShaderStage::Compute:
                targetProfile = L"cs_" + ToWideString(desc.shaderModel);
                break;
        }
        arguments.push_back(L"-T");
        arguments.push_back(targetProfile);

        arguments.push_back(L"-O3");
        arguments.push_back(L"-WX");

#if defined(_DEBUG)
        arguments.push_back(L"-Zi");
        arguments.push_back(L"-Qembed_debug");
#else
        arguments.push_back(L"-Qstrip_debug");
        arguments.push_back(L"-Qstrip_reflect");
#endif

        for (const auto& extra : desc.extraArguments)
        {
            arguments.push_back(extra);
        }

        std::vector<LPCWSTR> argPointers;
        argPointers.reserve(arguments.size());
        for (const auto& arg : arguments)
        {
            argPointers.push_back(arg.c_str());
        }

        ComPtr<IDxcResult> compileResult;
        const HRESULT compileHr = m_compiler->Compile(
            &sourceBuffer,
            argPointers.data(),
            static_cast<UINT32>(argPointers.size()),
            m_includeHandler.Get(),
            IID_PPV_ARGS(&compileResult)
        );

        if (FAILED(compileHr))
        {
            return std::unexpected(std::format("DxcCompiler::Compile execution failed. Error: 0x{:08X}", static_cast<u32>(compileHr)));
        }

        HRESULT status = S_OK;
        compileResult->GetStatus(&status);

        ComPtr<IDxcBlobUtf8> errors;
        compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
        {
            std::string_view errText(errors->GetStringPointer(), errors->GetStringLength());
            if (FAILED(status))
            {
                RL_LOG_ERROR("DXC compilation failed for '{}':\n{}", desc.filePath.string(), errText);
                return std::unexpected(std::string(errText));
            }
            else
            {
                RL_LOG_WARN("DXC compilation warnings for '{}':\n{}", desc.filePath.string(), errText);
            }
        }

        if (FAILED(status))
        {
            return std::unexpected(std::format("DXC failed with exit code 0x{:08X}", static_cast<u32>(status)));
        }

        ComPtr<IDxcBlob> shaderBlob;
        compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
        if (!shaderBlob)
        {
            return std::unexpected("DXC compilation succeeded but no binary output was generated.");
        }

        std::string stagePrefix = "vs_";
        switch (desc.stage)
        {
            case ShaderStage::Vertex: stagePrefix = "vs_"; break;
            case ShaderStage::Pixel: stagePrefix = "ps_"; break;
            case ShaderStage::Compute: stagePrefix = "cs_"; break;
        }
        const auto profileStr = stagePrefix + desc.shaderModel;
        RL_LOG_INFO("Shader '{}' compiled successfully [{}:{}] (Size: {} bytes)",
                    desc.filePath.filename().string(),
                    desc.entryPoint,
                    profileStr,
                    shaderBlob->GetBufferSize());

        return shaderBlob;
    }

    std::expected<ComPtr<IDxcBlob>, std::string> ShaderCompiler::CompileVertexShader(
        const std::filesystem::path& filePath,
        std::string_view entryPoint,
        std::string_view shaderModel)
    {
        return CompileShader({
            .filePath = filePath,
            .entryPoint = std::string(entryPoint),
            .stage = ShaderStage::Vertex,
            .shaderModel = std::string(shaderModel)
        });
    }

    std::expected<ComPtr<IDxcBlob>, std::string> ShaderCompiler::CompilePixelShader(
        const std::filesystem::path& filePath,
        std::string_view entryPoint,
        std::string_view shaderModel)
    {
        return CompileShader({
            .filePath = filePath,
            .entryPoint = std::string(entryPoint),
            .stage = ShaderStage::Pixel,
            .shaderModel = std::string(shaderModel)
        });
    }

    std::expected<ComPtr<ID3DBlob>, std::string> ShaderCompiler::CompileShaderModel50(
        const std::filesystem::path& filePath,
        std::string_view entryPoint,
        ShaderStage stage)
    {
        if (!std::filesystem::exists(filePath))
        {
            return std::unexpected(std::format("Shader source file does not exist: '{}'", filePath.string()));
        }

        std::string targetProfile;
        switch (stage)
        {
            case ShaderStage::Vertex:  targetProfile = "vs_5_0"; break;
            case ShaderStage::Pixel:   targetProfile = "ps_5_0"; break;
            case ShaderStage::Compute: targetProfile = "cs_5_0"; break;
        }

        UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;

        const std::wstring widePath = filePath.wstring();
        HRESULT hr = D3DCompileFromFile(
            widePath.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.data(),
            targetProfile.c_str(),
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            std::string errorMsg = "Unknown compilation error";
            if (errorBlob)
            {
                errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
            }
            RL_LOG_ERROR("D3DCompile (SM 5.0) failed for '{}': {}", filePath.string(), errorMsg);
            return std::unexpected(errorMsg);
        }

        RL_LOG_INFO("Shader '{}' compiled successfully via D3DCompile [{}:{}] (Size: {} bytes)",
                    filePath.filename().string(),
                    entryPoint,
                    targetProfile,
                    shaderBlob->GetBufferSize());

        return shaderBlob;
    }
}
