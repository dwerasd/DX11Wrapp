// DX11Device.h: DX11 디바이스 관리 (스왑체인, RTV, 블렌드/샘플러 스테이트)
#pragma once

#include "DX11Def.h"


namespace dx11
{
	class C_DX11_DEVICE
	{
	private:
		Microsoft::WRL::ComPtr<ID3D11Device>           m_pDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_pContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain1>        m_pSwapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;
		Microsoft::WRL::ComPtr<ID3D11BlendState>       m_pAlphaBlend;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>     m_pPointSampler;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>     m_pLinearSampler;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState>  m_pRasterState;

		HWND m_hWnd;
		UINT m_uWidth;
		UINT m_uHeight;
		bool m_bWindowed;
		bool m_bInitialized;

		bool createSwapChain();
		bool createRTV();
		void setViewport();

	public:
		C_DX11_DEVICE();
		~C_DX11_DEVICE();

		bool Initialize(HWND _hWnd, UINT _uWidth, UINT _uHeight, bool _bWindowed);
		void Shutdown();

		void BeginFrame(float _fR, float _fG, float _fB, float _fA);
		void EndFrame(UINT _uSync);
		bool Resize(UINT _uWidth, UINT _uHeight);

		// 접근자
		ID3D11Device* GetDevice()    const { return m_pDevice.Get(); }
		ID3D11DeviceContext* GetContext()   const { return m_pContext.Get(); }
		IDXGISwapChain1* GetSwapChain() const { return m_pSwapChain.Get(); }
		UINT GetWidth()  const { return m_uWidth; }
		UINT GetHeight() const { return m_uHeight; }
		bool IsInitialized() const { return m_bInitialized; }
		void SetLinearSampler(bool _bLinear);
	};
} // namespace dx11
