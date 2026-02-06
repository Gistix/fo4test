#include "ShaderUtils.h"

#include <d3dcompiler.h>

ID3D11DeviceChild* ShaderUtils::CompileShader(const wchar_t* FilePath, const char* ProgramType, const char* Program)
{
	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	auto device = reinterpret_cast<ID3D11Device*>(rendererData->device);

	// Compiler setup
	uint32_t flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

	ID3DBlob* shaderBlob;
	ID3DBlob* shaderErrors;

	std::string str;
	std::wstring path{ FilePath };
	std::transform(path.begin(), path.end(), std::back_inserter(str), [](wchar_t c) {
		return (char)c;
		});
	if (!std::filesystem::exists(FilePath)) {
		logger::error("Failed to compile shader; {} does not exist", str);
		return nullptr;
	}
	if (FAILED(D3DCompileFromFile(FilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, Program, ProgramType, flags, 0, &shaderBlob, &shaderErrors))) {
		logger::warn("Shader compilation failed:\n\n{}", shaderErrors ? static_cast<char*>(shaderErrors->GetBufferPointer()) : "Unknown error");
		return nullptr;
	}
	if (shaderErrors)
		logger::debug("Shader logs:\n{}", static_cast<char*>(shaderErrors->GetBufferPointer()));

	ID3D11ComputeShader* regShader;
	DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader));
	return regShader;
}