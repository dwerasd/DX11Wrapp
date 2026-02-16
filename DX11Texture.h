// DX11Texture.h: 텍스처 로드 (WIC 파일, 메모리 생성)
#pragma once

#include "DX11Def.h"


namespace dx11
{

	// WIC 기반 파일 텍스처 로드
	bool LoadTextureFromFile(
		ID3D11Device* _pDevice,
		const wchar_t* _pPath,
		ID3D11ShaderResourceView** _ppSRV,
		uint32_t* _pWidth,
		uint32_t* _pHeight
	);

	// 메모리에서 텍스처 생성 (RGBA 32비트)
	bool CreateTextureFromMemory(
		ID3D11Device* _pDevice,
		const void* _pData,
		uint32_t _uWidth,
		uint32_t _uHeight,
		ID3D11ShaderResourceView** _ppSRV
	);

} // namespace dx11
