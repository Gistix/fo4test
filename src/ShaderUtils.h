#pragma once

#include "Buffer.h"

struct ShaderUtils {
	static ID3D11DeviceChild* CompileShader(const wchar_t* FilePath, const char* ProgramType, const char* Program = "main");
};