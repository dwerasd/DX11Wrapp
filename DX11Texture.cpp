// DX11Texture.cpp: 텍스처 로드 (WIC 파일 로드, 메모리 생성)
#include "framework.h"
#include "DX11Texture.h"

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")


namespace dx11
{

	bool CreateTextureFromMemory(
		ID3D11Device* _pDevice,
		const void* _pData,
		uint32_t _uWidth,
		uint32_t _uHeight,
		ID3D11ShaderResourceView** _ppSRV)
	{
		if (!_pDevice || !_pData || !_ppSRV)
			return false;

		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = _uWidth;
		texDesc.Height = _uHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = _pData;
		initData.SysMemPitch = _uWidth * 4;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
		HRESULT hr = _pDevice->CreateTexture2D(&texDesc, &initData, &pTexture);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Tex] CreateTexture2D 실패: 0x%08X", hr);
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		hr = _pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, _ppSRV);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Tex] SRV 생성 실패: 0x%08X", hr);
			return false;
		}
		return true;
	}

	bool LoadTextureFromFile(
		ID3D11Device* _pDevice,
		const wchar_t* _pPath,
		ID3D11ShaderResourceView** _ppSRV,
		uint32_t* _pWidth,
		uint32_t* _pHeight)
	{
		if (!_pDevice || !_pPath || !_ppSRV)
			return false;

		// WIC 팩토리
		Microsoft::WRL::ComPtr<IWICImagingFactory> pWICFactory;
		HRESULT hr = ::CoCreateInstance(
			CLSID_WICImagingFactory, nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IWICImagingFactory),
			&pWICFactory
		);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Tex] WIC 팩토리 생성 실패: 0x%08X", hr);
			return false;
		}

		// 디코더
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
		hr = pWICFactory->CreateDecoderFromFilename(
			_pPath, nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnDemand, &pDecoder
		);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Tex] 파일 디코딩 실패: %s", _pPath);
			return false;
		}

		// 첫 프레임
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pFrame;
		hr = pDecoder->GetFrame(0, &pFrame);
		if (FAILED(hr)) return false;

		UINT uWidth = 0, uHeight = 0;
		pFrame->GetSize(&uWidth, &uHeight);

		// RGBA 32비트 변환
		Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;
		hr = pWICFactory->CreateFormatConverter(&pConverter);
		if (FAILED(hr)) return false;

		hr = pConverter->Initialize(
			pFrame.Get(), GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone, nullptr, 0.0,
			WICBitmapPaletteTypeMedianCut
		);
		if (FAILED(hr)) return false;

		// 픽셀 복사
		const UINT uStride = uWidth * 4;
		const UINT uBufferSize = uStride * uHeight;
		std::vector<uint8_t> vPixels(uBufferSize);
		hr = pConverter->CopyPixels(nullptr, uStride, uBufferSize, vPixels.data());
		if (FAILED(hr)) return false;

		// 텍스처 생성
		const bool bResult = CreateTextureFromMemory(_pDevice, vPixels.data(), uWidth, uHeight, _ppSRV);
		if (bResult)
		{
			if (_pWidth)  *_pWidth = uWidth;
			if (_pHeight) *_pHeight = uHeight;
		}
		return bResult;
	}

} // namespace dx11
