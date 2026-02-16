// DX11Device.cpp: DX11 디바이스 생성, 스왑체인, 렌더 스테이트 관리
#include "framework.h"
#include "DX11Device.h"


namespace dx11
{

	C_DX11_DEVICE::C_DX11_DEVICE()
		: m_hWnd(nullptr)
		, m_uWidth(0)
		, m_uHeight(0)
		, m_bWindowed(true)
		, m_bInitialized(false)
	{
	}

	C_DX11_DEVICE::~C_DX11_DEVICE()
	{
		Shutdown();
	}

	bool C_DX11_DEVICE::Initialize(HWND _hWnd, UINT _uWidth, UINT _uHeight, bool _bWindowed)
	{
		m_hWnd = _hWnd;
		m_uWidth = _uWidth;
		m_uHeight = _uHeight;
		m_bWindowed = _bWindowed;

		// D3D11 디바이스 생성
		constexpr D3D_FEATURE_LEVEL aFeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
		UINT uFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;	// WIC 호환
#if defined(_DEBUG)
		uFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL eSelectedLevel{};
		Microsoft::WRL::ComPtr<ID3D11Device>        pDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;

		HRESULT hr = ::D3D11CreateDevice(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			uFlags, aFeatureLevels, 1,
			D3D11_SDK_VERSION,
			&pDevice, &eSelectedLevel, &pContext
		);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] D3D11CreateDevice 실패: 0x%08X", hr);
			return false;
		}

		m_pDevice = pDevice;
		m_pContext = pContext;

		// 스왑체인
		if (!createSwapChain())
			return false;

		// 렌더 타겟 뷰
		if (!createRTV())
			return false;

		// 블렌드 스테이트 (알파 블렌딩)
		D3D11_BLEND_DESC blendDesc{};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		hr = m_pDevice->CreateBlendState(&blendDesc, &m_pAlphaBlend);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] BlendState 생성 실패: 0x%08X", hr);
			return false;
		}

		// 포인트 샘플러 (픽셀아트용)
		D3D11_SAMPLER_DESC samplerDesc{};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_pDevice->CreateSamplerState(&samplerDesc, &m_pPointSampler);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] SamplerState 생성 실패: 0x%08X", hr);
			return false;
		}

		// 리니어 샘플러 (폰트/UI용)
		D3D11_SAMPLER_DESC linearDesc{};
		linearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		linearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		linearDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_pDevice->CreateSamplerState(&linearDesc, &m_pLinearSampler);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] LinearSampler 생성 실패: 0x%08X", hr);
			return false;
		}

		// 래스터라이저 (2D용: 컬링 없음)
		D3D11_RASTERIZER_DESC rasterDesc{};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.ScissorEnable = FALSE;
		rasterDesc.DepthClipEnable = TRUE;

		hr = m_pDevice->CreateRasterizerState(&rasterDesc, &m_pRasterState);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] RasterizerState 생성 실패: 0x%08X", hr);
			return false;
		}

		setViewport();
		m_bInitialized = true;
		DBGPRINT(L"[DX11] 디바이스 초기화 완료: %ux%u", m_uWidth, m_uHeight);
		return true;
	}

	bool C_DX11_DEVICE::createSwapChain()
	{
		Microsoft::WRL::ComPtr<IDXGIDevice1> pDXGIDevice;
		HRESULT hr = m_pDevice.As(&pDXGIDevice);
		if (FAILED(hr)) return false;

		Microsoft::WRL::ComPtr<IDXGIAdapter> pAdapter;
		hr = pDXGIDevice->GetAdapter(&pAdapter);
		if (FAILED(hr)) return false;

		Microsoft::WRL::ComPtr<IDXGIFactory2> pFactory;
		hr = pAdapter->GetParent(__uuidof(IDXGIFactory2), &pFactory);
		if (FAILED(hr)) return false;

		DXGI_SWAP_CHAIN_DESC1 scDesc{};
		scDesc.Width = m_uWidth;
		scDesc.Height = m_uHeight;
		scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scDesc.SampleDesc.Count = 1;
		scDesc.SampleDesc.Quality = 0;
		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount = 2;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		scDesc.Flags = 0;

		hr = pFactory->CreateSwapChainForHwnd(
			m_pDevice.Get(), m_hWnd, &scDesc, nullptr, nullptr, &m_pSwapChain
		);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] 스왑체인 생성 실패: 0x%08X", hr);
			return false;
		}

		// ALT+Enter 전체화면 전환 비활성화
		pFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
		return true;
	}

	bool C_DX11_DEVICE::createRTV()
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
		HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer);
		if (FAILED(hr)) return false;

		hr = m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRTV);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] RTV 생성 실패: 0x%08X", hr);
			return false;
		}
		return true;
	}

	void C_DX11_DEVICE::setViewport()
	{
		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(m_uWidth);
		vp.Height = static_cast<float>(m_uHeight);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		m_pContext->RSSetViewports(1, &vp);
	}

	void C_DX11_DEVICE::Shutdown()
	{
		if (!m_bInitialized) return;

		if (m_pContext)
			m_pContext->ClearState();

		m_pRasterState.Reset();
		m_pPointSampler.Reset();
		m_pAlphaBlend.Reset();
		m_pRTV.Reset();
		m_pSwapChain.Reset();
		m_pContext.Reset();
		m_pDevice.Reset();

		m_bInitialized = false;
		DBGPRINT(L"[DX11] 디바이스 종료");
	}

	void C_DX11_DEVICE::BeginFrame(float _fR, float _fG, float _fB, float _fA)
	{
		const float aClearColor[4] = { _fR, _fG, _fB, _fA };
		m_pContext->ClearRenderTargetView(m_pRTV.Get(), aClearColor);
		m_pContext->OMSetRenderTargets(1, m_pRTV.GetAddressOf(), nullptr);
		m_pContext->OMSetBlendState(m_pAlphaBlend.Get(), nullptr, 0xFFFFFFFF);
		m_pContext->RSSetState(m_pRasterState.Get());
		m_pContext->PSSetSamplers(0, 1, m_pPointSampler.GetAddressOf());
		setViewport();
	}

	void C_DX11_DEVICE::EndFrame(UINT _uSync)
	{
		m_pSwapChain->Present(_uSync, 0);
	}

	bool C_DX11_DEVICE::Resize(UINT _uWidth, UINT _uHeight)
	{
		if (!m_bInitialized || (_uWidth == 0) || (_uHeight == 0))
			return false;

		m_uWidth = _uWidth;
		m_uHeight = _uHeight;

		m_pContext->OMSetRenderTargets(0, nullptr, nullptr);
		m_pRTV.Reset();

		const HRESULT hr = m_pSwapChain->ResizeBuffers(0, _uWidth, _uHeight, DXGI_FORMAT_UNKNOWN, 0);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11] ResizeBuffers 실패: 0x%08X", hr);
			return false;
		}

		if (!createRTV())
			return false;

		setViewport();
		return true;
	}

	void C_DX11_DEVICE::SetLinearSampler(bool _bLinear)
	{
		if (!m_pContext) return;
		if (_bLinear)
			m_pContext->PSSetSamplers(0, 1, m_pLinearSampler.GetAddressOf());
		else
			m_pContext->PSSetSamplers(0, 1, m_pPointSampler.GetAddressOf());
	}

} // namespace dx11
