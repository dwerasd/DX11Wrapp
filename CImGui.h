// CImGui.h: DX11 백엔드 ImGui wrapper.
// DX9Wrapp/CImGui.h 의 DX11 포팅. 인터페이스(API 시그니처)는 가능한 한 동일하게
// 유지하여 C_MAIN(상속자) 측 호출 코드 변경을 최소화한다.
//
// 차이점 (DX9 -> DX11):
// - LPDIRECT3DDEVICE9              -> ID3D11Device*
// - D3DPRESENT_PARAMETERS          -> DXGI_SWAP_CHAIN_DESC
// - ImGui_ImplDX9_*                -> ImGui_ImplDX11_*
// - device->Clear/BeginScene/Present 단일 호출 -> context + swapchain 분리
// - VSync 는 IDXGISwapChain::Present(syncInterval, 0) 의 syncInterval 로 제어.
#pragma once


#include <unordered_map>

#include <DarkCore/DDef.h>
#include <DarkCore/DTypes.h>
#include <DarkCore/DColor.h>
#include <DarkCore/DString.h>
#include <DarkCore/DUtil.h>
#include <DarkCore/DLocale.h>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_impl_dx11.h>
#include <ImGui/imgui_impl_win32.h>

#include <ImGui/implot.h>
#include <ImGui/implot_internal.h>
#pragma comment(lib, "ImGui")

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#include "DX11Def.h"

#define _USE_LIB_IMGUI_

class C_IMGUI
{
protected:
	bool bVerticalSync{ false }, bResetDevice{ false };	// 수직동기화

	HWND                     hWnd{ nullptr };
	ID3D11Device*            pDevice{ nullptr };
	ID3D11DeviceContext*     pContext{ nullptr };
	IDXGISwapChain*          pSwapChain{ nullptr };
	ID3D11RenderTargetView*  pMainRTV{ nullptr };

	dk::_DCOLOR dkClearColor{};

	ImVec2 v2MainWindowSize{};	// 이건 ImGui 용

	// 내부 — backbuffer RTV 생성/해제
	void CreateMainRTV_();
	void ReleaseMainRTV_();

public:
	explicit C_IMGUI(bool _bVerticalSync = true);
	virtual ~C_IMGUI();

	// 두 번째 인자는 DX9 시절 시그니처 호환을 위해 유지. 외부에서 device 를 미리
	// 만들어 주입하는 용도가 없으면 nullptr 로 호출 (내부에서 device 생성).
	void Init_ImGui(HWND _hWnd, ID3D11Device* _pDevice = nullptr, bool _bVerticalSync = true);
	virtual long Update_ImGui();
	void Draw_ImGui();
	void Destroy_ImGui();

	// ResetDevice: WM_SIZE 시 swapchain ResizeBuffers + RTV 재생성.
	void ResetDevice();
	void SetWindowSize(UINT _x, UINT _y);

	bool WndProc_ImGui(HWND _hWnd, UINT _nMessage, WPARAM _wParam, LPARAM _lParam);
};


LPCSTR toUtf8(LPCSTR _pszData);
LPCSTR toUtf8(LPCWSTR _pwszData);

LPCSTR GetColorName(size_t _nIndex);
ImU32 GetColorOfText(LPCSTR _pColorName);
ImVec4 GetColorVec4OfText(LPCSTR _pColorName);
