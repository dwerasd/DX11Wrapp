// DX11SpriteEngine.cpp: 2D 인스턴스드 스프라이트 배칭 엔진
// DX9Wrapp C_DX9_SPRITE_ENGINE 패턴 계승 (싱글톤, 텍스처 슬롯, 배칭 플러시)
#include "framework.h"
#include "DX11SpriteEngine.h"

#include <cstring>


//============================================================================
// 셰이더 소스 (문자열 임베드)
//============================================================================
static const char* const g_pSpriteVS = R"HLSL(
cbuffer CBPerFrame : register(b0)
{
    float2 g_InvScreenSize;
    float2 g_Pad;
};

struct VS_INPUT
{
    float2 Pos         : POSITION;
    float4 InstPosSize : INST_POS;
    float4 InstUV      : INST_UV;
    float  InstDepth   : INST_DEPTH;
    uint   InstColor   : INST_COLOR;
};

struct VS_OUTPUT
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float2 pixelPos;
    pixelPos.x = input.InstPosSize.x + input.Pos.x * input.InstPosSize.z;
    pixelPos.y = input.InstPosSize.y + input.Pos.y * input.InstPosSize.w;

    output.Pos.x = pixelPos.x * g_InvScreenSize.x * 2.0 - 1.0;
    output.Pos.y = 1.0 - pixelPos.y * g_InvScreenSize.y * 2.0;
    output.Pos.z = input.InstDepth;
    output.Pos.w = 1.0;

    output.UV.x = input.InstUV.x + input.Pos.x * (input.InstUV.z - input.InstUV.x);
    output.UV.y = input.InstUV.y + input.Pos.y * (input.InstUV.w - input.InstUV.y);

    output.Color.a = float((input.InstColor >> 24) & 0xFF) / 255.0;
    output.Color.r = float((input.InstColor >> 16) & 0xFF) / 255.0;
    output.Color.g = float((input.InstColor >>  8) & 0xFF) / 255.0;
    output.Color.b = float((input.InstColor >>  0) & 0xFF) / 255.0;

    return output;
}
)HLSL";

static const char* const g_pSpritePS = R"HLSL(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_INPUT
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.UV);
    float4 result = texColor * input.Color;
    clip(result.a - 0.01);
    return result;
}
)HLSL";


namespace dx11
{

	// 싱글톤 인스턴스
	C_DX11_SPRITE_ENGINE* C_DX11_SPRITE_ENGINE::s_pInstance = nullptr;

	C_DX11_SPRITE_ENGINE::C_DX11_SPRITE_ENGINE()
		: m_pInstances(nullptr)
		, m_uInstanceCount(0)
		, m_hCurrentTexture(INVALID_TEXTURE)
		, m_bInitialized(false)
	{
	}

	C_DX11_SPRITE_ENGINE::~C_DX11_SPRITE_ENGINE()
	{
		Shutdown();
	}

	C_DX11_SPRITE_ENGINE* C_DX11_SPRITE_ENGINE::GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new C_DX11_SPRITE_ENGINE();
		return s_pInstance;
	}

	void C_DX11_SPRITE_ENGINE::DestroyInstance()
	{
		if (s_pInstance)
		{
			delete s_pInstance;
			s_pInstance = nullptr;
		}
	}

	//============================================================================
	// 초기화
	//============================================================================
	bool C_DX11_SPRITE_ENGINE::Initialize(HWND _hWnd, UINT _uWidth, UINT _uHeight, bool _bWindowed)
	{
		// 디바이스 초기화
		if (!m_Device.Initialize(_hWnd, _uWidth, _uHeight, _bWindowed))
			return false;

		// 셰이더 컴파일
		if (!compileShaders())
			return false;

		// 버퍼 생성
		if (!createBuffers())
			return false;

		// 인스턴스 CPU 스테이징 버퍼
		m_pInstances = new SpriteInstance[MAX_INSTANCES];
		std::memset(m_pInstances, 0, sizeof(SpriteInstance) * MAX_INSTANCES);
		m_uInstanceCount = 0;

		// 텍스처 슬롯 예약
		m_vTextures.reserve(256);

		m_bInitialized = true;
		DBGPRINT(L"[DX11Engine] 스프라이트 엔진 초기화 완료");
		return true;
	}

	void C_DX11_SPRITE_ENGINE::Shutdown()
	{
		if (!m_bInitialized) return;

		// 텍스처 해제
		m_vTextures.clear();

		// 인스턴스 버퍼 해제
		delete[] m_pInstances;
		m_pInstances = nullptr;

		// GPU 리소스 해제
		m_pConstantBuffer.Reset();
		m_pInstanceBuffer.Reset();
		m_pIndexBuffer.Reset();
		m_pVertexBuffer.Reset();
		m_pInputLayout.Reset();
		m_pPS.Reset();
		m_pVS.Reset();

		m_Device.Shutdown();

		m_bInitialized = false;
		DBGPRINT(L"[DX11Engine] 스프라이트 엔진 종료");
	}

	//============================================================================
	// 셰이더 컴파일
	//============================================================================
	bool C_DX11_SPRITE_ENGINE::compileShaders()
	{
		ID3D11Device* const pDevice = m_Device.GetDevice();

		// VS 컴파일
		Microsoft::WRL::ComPtr<ID3DBlob> pVSBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

		HRESULT hr = ::D3DCompile(
			g_pSpriteVS, std::strlen(g_pSpriteVS),
			"SpriteVS", nullptr, nullptr,
			"main", "vs_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&pVSBlob, &pErrorBlob
		);
		if (FAILED(hr))
		{
			if (pErrorBlob)
				DBGPRINT(L"[DX11Engine] VS 컴파일 에러: %S", (const char*)pErrorBlob->GetBufferPointer());
			return false;
		}

		hr = pDevice->CreateVertexShader(
			pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
			nullptr, &m_pVS
		);
		if (FAILED(hr)) return false;

		// PS 컴파일
		Microsoft::WRL::ComPtr<ID3DBlob> pPSBlob;
		hr = ::D3DCompile(
			g_pSpritePS, std::strlen(g_pSpritePS),
			"SpritePS", nullptr, nullptr,
			"main", "ps_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&pPSBlob, &pErrorBlob
		);
		if (FAILED(hr))
		{
			if (pErrorBlob)
				DBGPRINT(L"[DX11Engine] PS 컴파일 에러: %S", (const char*)pErrorBlob->GetBufferPointer());
			return false;
		}

		hr = pDevice->CreatePixelShader(
			pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
			nullptr, &m_pPS
		);
		if (FAILED(hr)) return false;

		// 인풋 레이아웃
		const D3D11_INPUT_ELEMENT_DESC aLayout[] =
		{
			// Per-vertex (slot 0)
			{ "POSITION",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA,   0 },
			// Per-instance (slot 1)
			{ "INST_POS",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_UV",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_DEPTH", 0, DXGI_FORMAT_R32_FLOAT,          1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_COLOR", 0, DXGI_FORMAT_R32_UINT,           1, 36, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};

		hr = pDevice->CreateInputLayout(
			aLayout, _countof(aLayout),
			pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
			&m_pInputLayout
		);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Engine] InputLayout 생성 실패: 0x%08X", hr);
			return false;
		}

		DBGPRINT(L"[DX11Engine] 셰이더 컴파일 완료");
		return true;
	}

	//============================================================================
	// 버퍼 생성
	//============================================================================
	bool C_DX11_SPRITE_ENGINE::createBuffers()
	{
		ID3D11Device* const pDevice = m_Device.GetDevice();

		// 유닛 쿼드 정점 (float2 × 4)
		constexpr float aQuadVerts[] =
		{
			0.0f, 0.0f,		// 좌상
			1.0f, 0.0f,		// 우상
			0.0f, 1.0f,		// 좌하
			1.0f, 1.0f,		// 우하
		};

		D3D11_BUFFER_DESC vbDesc{};
		vbDesc.ByteWidth = sizeof(aQuadVerts);
		vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vbData{};
		vbData.pSysMem = aQuadVerts;

		HRESULT hr = pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);
		if (FAILED(hr)) return false;

		// 인덱스 버퍼 (2 삼각형)
		const uint16_t aIndices[] = { 0, 1, 2, 2, 1, 3 };

		D3D11_BUFFER_DESC ibDesc{};
		ibDesc.ByteWidth = sizeof(aIndices);
		ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA ibData{};
		ibData.pSysMem = aIndices;

		hr = pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
		if (FAILED(hr)) return false;

		// 인스턴스 버퍼 (동적, 최대 MAX_INSTANCES)
		D3D11_BUFFER_DESC instDesc{};
		instDesc.ByteWidth = sizeof(SpriteInstance) * MAX_INSTANCES;
		instDesc.Usage = D3D11_USAGE_DYNAMIC;
		instDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		instDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = pDevice->CreateBuffer(&instDesc, nullptr, &m_pInstanceBuffer);
		if (FAILED(hr)) return false;

		// 상수 버퍼
		D3D11_BUFFER_DESC cbDesc{};
		cbDesc.ByteWidth = sizeof(CBPerFrame);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer);
		if (FAILED(hr)) return false;

		DBGPRINT(L"[DX11Engine] 버퍼 생성 완료 (인스턴스 최대: %u)", MAX_INSTANCES);
		return true;
	}

	//============================================================================
	// 텍스처 관리
	//============================================================================
	TextureHandle C_DX11_SPRITE_ENGINE::LoadTexture(const wchar_t* _pPath)
	{
		if (!_pPath) return INVALID_TEXTURE;

		TextureSlot slot;
		if (!dx11::LoadTextureFromFile(
			m_Device.GetDevice(), _pPath,
			slot.m_pSRV.GetAddressOf(),
			&slot.m_uWidth, &slot.m_uHeight))
		{
			DBGPRINT(L"[DX11Engine] 텍스처 로드 실패: %s", _pPath);
			return INVALID_TEXTURE;
		}

		slot.m_bManaged = true;
		const TextureHandle hTex = static_cast<TextureHandle>(m_vTextures.size());
		m_vTextures.push_back(std::move(slot));
		return hTex;
	}

	TextureHandle C_DX11_SPRITE_ENGINE::CreateTexture(const void* _pData, UINT _uW, UINT _uH)
	{
		if (!_pData || (_uW == 0) || (_uH == 0))
			return INVALID_TEXTURE;

		TextureSlot slot;
		if (!dx11::CreateTextureFromMemory(
			m_Device.GetDevice(), _pData, _uW, _uH,
			slot.m_pSRV.GetAddressOf()))
		{
			return INVALID_TEXTURE;
		}

		slot.m_uWidth = _uW;
		slot.m_uHeight = _uH;
		slot.m_bManaged = true;
		const TextureHandle hTex = static_cast<TextureHandle>(m_vTextures.size());
		m_vTextures.push_back(std::move(slot));
		return hTex;
	}

	void C_DX11_SPRITE_ENGINE::ReleaseTexture(TextureHandle _hTex)
	{
		if ((_hTex < 0) || (_hTex >= static_cast<TextureHandle>(m_vTextures.size())))
			return;
		m_vTextures[_hTex].m_pSRV.Reset();
		m_vTextures[_hTex].m_bManaged = false;
	}

	//============================================================================
	// 렌더링
	//============================================================================
	void C_DX11_SPRITE_ENGINE::BeginFrame(float _fR, float _fG, float _fB, float _fA)
	{
		m_Device.BeginFrame(_fR, _fG, _fB, _fA);

		ID3D11DeviceContext* const pCtx = m_Device.GetContext();

		// 상수 버퍼 갱신
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = pCtx->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			CBPerFrame* const pCB = static_cast<CBPerFrame*>(mapped.pData);
			pCB->m_fInvScreenWidth = 1.0f / static_cast<float>(m_Device.GetWidth());
			pCB->m_fInvScreenHeight = 1.0f / static_cast<float>(m_Device.GetHeight());
			pCB->m_fPad[0] = 0.0f;
			pCB->m_fPad[1] = 0.0f;
			pCtx->Unmap(m_pConstantBuffer.Get(), 0);
		}

		// 파이프라인 바인드
		pCtx->IASetInputLayout(m_pInputLayout.Get());
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 유닛 쿼드 VB (slot 0)
		constexpr UINT uVertStride = sizeof(float) * 2;
		constexpr UINT uVertOffset = 0;
		ID3D11Buffer* const pVB = m_pVertexBuffer.Get();
		pCtx->IASetVertexBuffers(0, 1, &pVB, &uVertStride, &uVertOffset);

		// 인덱스 버퍼
		pCtx->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

		// 셰이더
		pCtx->VSSetShader(m_pVS.Get(), nullptr, 0);
		pCtx->PSSetShader(m_pPS.Get(), nullptr, 0);
		pCtx->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());

		// 상태 초기화
		m_uInstanceCount = 0;
		m_hCurrentTexture = INVALID_TEXTURE;
		m_Stats.m_uDrawCallsPerFrame = 0;
		m_Stats.m_uSpritesPerFrame = 0;
		m_Stats.m_uTextureChanges = 0;
	}

	void C_DX11_SPRITE_ENGINE::Begin2D()
	{
		ID3D11DeviceContext* const pCtx = m_Device.GetContext();

		// 2D 스테이트: 뎁스 OFF, 컬링 OFF, 알파 블렌드, 포인트 샘플러
		m_Device.Set2DState();

		// 상수 버퍼 갱신
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = pCtx->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			CBPerFrame* const pCB = static_cast<CBPerFrame*>(mapped.pData);
			pCB->m_fInvScreenWidth = 1.0f / static_cast<float>(m_Device.GetWidth());
			pCB->m_fInvScreenHeight = 1.0f / static_cast<float>(m_Device.GetHeight());
			pCB->m_fPad[0] = 0.0f;
			pCB->m_fPad[1] = 0.0f;
			pCtx->Unmap(m_pConstantBuffer.Get(), 0);
		}

		// 2D 파이프라인 바인드
		pCtx->IASetInputLayout(m_pInputLayout.Get());
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		constexpr UINT uVertStride = sizeof(float) * 2;
		constexpr UINT uVertOffset = 0;
		ID3D11Buffer* const pVB = m_pVertexBuffer.Get();
		pCtx->IASetVertexBuffers(0, 1, &pVB, &uVertStride, &uVertOffset);
		pCtx->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		pCtx->VSSetShader(m_pVS.Get(), nullptr, 0);
		pCtx->PSSetShader(m_pPS.Get(), nullptr, 0);
		pCtx->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());

		m_uInstanceCount = 0;
		m_hCurrentTexture = INVALID_TEXTURE;
		m_Stats.m_uDrawCallsPerFrame = 0;
		m_Stats.m_uSpritesPerFrame = 0;
		m_Stats.m_uTextureChanges = 0;
	}

	void C_DX11_SPRITE_ENGINE::Draw(
		TextureHandle _hTex,
		float _fX, float _fY, float _fW, float _fH,
		float _fU0, float _fV0, float _fU1, float _fV1,
		float _fDepth, uint32_t _uColor)
	{
		// 텍스처 변경 시 현재 배치 플러시
		if ((_hTex != m_hCurrentTexture) && (m_uInstanceCount > 0))
			flush();

		m_hCurrentTexture = _hTex;

		// 버퍼 풀 시 플러시
		if (m_uInstanceCount >= MAX_INSTANCES)
			flush();

		// 인스턴스 추가
		{
			SpriteInstance& inst = m_pInstances[m_uInstanceCount];
			inst.m_ScreenX = _fX;
			inst.m_ScreenY = _fY;
			inst.m_Width = _fW;
			inst.m_Height = _fH;
			inst.m_UVLeft = _fU0;
			inst.m_UVTop = _fV0;
			inst.m_UVRight = _fU1;
			inst.m_UVBottom = _fV1;
			inst.m_Depth = _fDepth;
			inst.m_Color = _uColor;
			inst.m_Pad[0] = 0.0f;
			inst.m_Pad[1] = 0.0f;
		}
		++m_uInstanceCount;
		++m_Stats.m_uSpritesPerFrame;
	}

	void C_DX11_SPRITE_ENGINE::flush()
	{
		if (m_uInstanceCount == 0) return;

		ID3D11DeviceContext* const pCtx = m_Device.GetContext();

		// 인스턴스 데이터 GPU 전송
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = pCtx->Map(m_pInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr)) return;

		std::memcpy(mapped.pData, m_pInstances, sizeof(SpriteInstance) * m_uInstanceCount);
		pCtx->Unmap(m_pInstanceBuffer.Get(), 0);

		// 인스턴스 VB 바인드 (slot 1)
		constexpr UINT uInstStride = sizeof(SpriteInstance);
		constexpr UINT uInstOffset = 0;
		ID3D11Buffer* const pInstBuf = m_pInstanceBuffer.Get();
		pCtx->IASetVertexBuffers(1, 1, &pInstBuf, &uInstStride, &uInstOffset);

		// 텍스처 바인드
		if ((m_hCurrentTexture >= 0) &&
			(m_hCurrentTexture < static_cast<TextureHandle>(m_vTextures.size())) &&
			m_vTextures[m_hCurrentTexture].m_pSRV)
		{
			ID3D11ShaderResourceView* const pSRV = m_vTextures[m_hCurrentTexture].m_pSRV.Get();
			pCtx->PSSetShaderResources(0, 1, &pSRV);
			++m_Stats.m_uTextureChanges;
		}

		// 인스턴스드 드로우
		pCtx->DrawIndexedInstanced(6, m_uInstanceCount, 0, 0, 0);

		++m_Stats.m_uDrawCallsPerFrame;
		m_uInstanceCount = 0;
	}

	void C_DX11_SPRITE_ENGINE::EndFrame()
	{
		// 잔여 인스턴스 플러시
		if (m_uInstanceCount > 0)
			flush();

		++m_Stats.m_uFrameCount;
	}

	void C_DX11_SPRITE_ENGINE::Present(UINT _uSync)
	{
		m_Device.EndFrame(_uSync);
	}

	//============================================================================
	// 스크린샷 (백버퍼 → BMP 파일)
	//============================================================================
	bool C_DX11_SPRITE_ENGINE::SaveScreenshot(const wchar_t* _pPath)
	{
		if (!_pPath || !m_bInitialized) return false;

		IDXGISwapChain1* const pSwapChain = m_Device.GetSwapChain();
		ID3D11Device* const pDevice = m_Device.GetDevice();
		ID3D11DeviceContext* const pCtx = m_Device.GetContext();
		if (!pSwapChain || !pDevice || !pCtx) return false;

		// 백버퍼 획득
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
		HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer);
		if (FAILED(hr)) return false;

		D3D11_TEXTURE2D_DESC desc{};
		pBackBuffer->GetDesc(&desc);

		// 스테이징 텍스처 (CPU 읽기용)
		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pStaging;
		hr = pDevice->CreateTexture2D(&stagingDesc, nullptr, &pStaging);
		if (FAILED(hr)) return false;

		pCtx->CopyResource(pStaging.Get(), pBackBuffer.Get());

		// Map → 픽셀 읽기
		D3D11_MAPPED_SUBRESOURCE mapped{};
		hr = pCtx->Map(pStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) return false;

		const UINT uW = desc.Width;
		const UINT uH = desc.Height;

		// BMP 파일 저장 (R8G8B8A8 → BGR 24bit)
		const UINT uRowBytes = uW * 3;
		const UINT uPadding = (4 - (uRowBytes % 4)) % 4;
		const UINT uStride = uRowBytes + uPadding;
		const UINT uDataSize = uStride * uH;

		BITMAPFILEHEADER bfh{};
		bfh.bfType = 0x4D42;	// 'BM'
		bfh.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + uDataSize);
		bfh.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));

		BITMAPINFOHEADER bih{};
		bih.biSize = sizeof(BITMAPINFOHEADER);
		bih.biWidth = static_cast<LONG>(uW);
		bih.biHeight = static_cast<LONG>(uH);	// bottom-up
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biCompression = BI_RGB;
		bih.biSizeImage = uDataSize;

		const HANDLE hFile = ::CreateFileW(_pPath, GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			pCtx->Unmap(pStaging.Get(), 0);
			return false;
		}

		DWORD dwWritten = 0;
		::WriteFile(hFile, &bfh, sizeof(bfh), &dwWritten, nullptr);
		::WriteFile(hFile, &bih, sizeof(bih), &dwWritten, nullptr);

		// 픽셀 변환: top-down RGBA → bottom-up BGR
		constexpr uint8_t uZeroPad[4] = {};
		for (UINT y = 0; y < uH; ++y)
		{
			const UINT uSrcRow = uH - 1 - y;	// BMP는 bottom-up
			const uint8_t* const pSrcRow = static_cast<const uint8_t*>(mapped.pData)
				+ uSrcRow * mapped.RowPitch;

			// 행 단위 BGR 변환 후 쓰기
			uint8_t aRowBuf[8192 * 3];	// 최대 8K 해상도
			for (UINT x = 0; x < uW; ++x)
			{
				const uint8_t uR = pSrcRow[x * 4 + 0];
				const uint8_t uG = pSrcRow[x * 4 + 1];
				const uint8_t uB = pSrcRow[x * 4 + 2];
				aRowBuf[x * 3 + 0] = uB;
				aRowBuf[x * 3 + 1] = uG;
				aRowBuf[x * 3 + 2] = uR;
			}
			::WriteFile(hFile, aRowBuf, uRowBytes, &dwWritten, nullptr);
			if (uPadding > 0)
				::WriteFile(hFile, uZeroPad, uPadding, &dwWritten, nullptr);
		}

		::CloseHandle(hFile);
		pCtx->Unmap(pStaging.Get(), 0);

		DBGPRINT(L"[DX11Engine] 스크린샷 저장: %s (%ux%u)", _pPath, uW, uH);
		return true;
	}

	//============================================================================
	// 동적 텍스처 (D3D11_USAGE_DEFAULT, UpdateSubresource 가능)
	//============================================================================
	TextureHandle C_DX11_SPRITE_ENGINE::CreateDynamicTexture(UINT _uW, UINT _uH)
	{
		if ((_uW == 0) || (_uH == 0)) return INVALID_TEXTURE;

		ID3D11Device* const pDevice = m_Device.GetDevice();

		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = _uW;
		texDesc.Height = _uH;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		// 검정 투명으로 초기화
		const UINT uPixelCount = _uW * _uH;
		std::vector<uint32_t> vClear(uPixelCount, 0);

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = vClear.data();
		initData.SysMemPitch = _uW * 4;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
		HRESULT hr = pDevice->CreateTexture2D(&texDesc, &initData, &pTexture);
		if (FAILED(hr)) return INVALID_TEXTURE;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		TextureSlot slot;
		hr = pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, &slot.m_pSRV);
		if (FAILED(hr)) return INVALID_TEXTURE;

		slot.m_pTexture = pTexture;
		slot.m_uWidth = _uW;
		slot.m_uHeight = _uH;
		slot.m_bManaged = true;

		const TextureHandle hTex = static_cast<TextureHandle>(m_vTextures.size());
		m_vTextures.push_back(std::move(slot));
		return hTex;
	}

	void C_DX11_SPRITE_ENGINE::UpdateTextureRegion(
		TextureHandle _hTex,
		UINT _uX, UINT _uY, UINT _uW, UINT _uH,
		const void* _pData)
	{
		if ((_hTex < 0) || (_hTex >= static_cast<TextureHandle>(m_vTextures.size())))
			return;

		const TextureSlot& slot = m_vTextures[_hTex];
		if (!slot.m_pTexture || !_pData) return;

		D3D11_BOX destBox;
		destBox.left = _uX;
		destBox.top = _uY;
		destBox.front = 0;
		destBox.right = _uX + _uW;
		destBox.bottom = _uY + _uH;
		destBox.back = 1;

		m_Device.GetContext()->UpdateSubresource(
			slot.m_pTexture.Get(), 0, &destBox,
			_pData, _uW * 4, 0);
	}

	void C_DX11_SPRITE_ENGINE::SetLinearFiltering(bool _bLinear)
	{
		// 현재 배치 플러시 (샘플러 변경 전)
		if (m_uInstanceCount > 0)
			flush();
		m_Device.SetLinearSampler(_bLinear);
	}

} // namespace dx11
