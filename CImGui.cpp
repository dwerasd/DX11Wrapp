// CImGui.cpp: DX11 백엔드 ImGui wrapper 구현.
// DX9Wrapp/CImGui.cpp 의 DX11 포팅. toUtf8 캐시, 색상 맵 등 ImGui 비의존 헬퍼는
// 원본을 그대로 가져온다. 렌더 경로만 DX11 (D3D11CreateDeviceAndSwapChain +
// ID3D11RenderTargetView + ImGui_ImplDX11_*) 로 교체.

#include "framework.h"
#include "CImGui.h"

#include <cstring>


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

C_IMGUI::C_IMGUI(bool _bVerticalSync)
	: bVerticalSync(_bVerticalSync)
	, dkClearColor(114, 140, 153)
{
}

C_IMGUI::~C_IMGUI()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 내부 — backbuffer 로부터 main RTV 생성/해제.
// ─────────────────────────────────────────────────────────────────────────────
void C_IMGUI::CreateMainRTV_()
{
	if (this->pSwapChain == nullptr || this->pDevice == nullptr) { return; }
	ID3D11Texture2D* pBackBuffer_ = nullptr;
	const HRESULT hr_ = this->pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer_));
	if (FAILED(hr_) || pBackBuffer_ == nullptr)
	{
		DBGPRINT(L"[ImGui] CreateMainRTV_: GetBuffer 실패 (0x%08X)", hr_);
		return;
	}
	this->pDevice->CreateRenderTargetView(pBackBuffer_, nullptr, &this->pMainRTV);
	pBackBuffer_->Release();
}

void C_IMGUI::ReleaseMainRTV_()
{
	if (this->pMainRTV != nullptr)
	{
		this->pMainRTV->Release();
		this->pMainRTV = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Init — D3D11 device + swapchain + ImGui backend.
// ─────────────────────────────────────────────────────────────────────────────
void C_IMGUI::Init_ImGui(HWND _hWnd, ID3D11Device* _pDevice, bool _bVerticalSync)
{
	this->hWnd          = _hWnd;
	this->bVerticalSync = _bVerticalSync;

	if (_pDevice == nullptr)
	{
		// SwapChain desc — DX9 의 D3DPRESENT_PARAMETERS 와 동치.
		DXGI_SWAP_CHAIN_DESC scd_{};
		scd_.BufferCount                        = 2;
		scd_.BufferDesc.Width                   = 0;   // 0 = HWND 클라 영역 자동
		scd_.BufferDesc.Height                  = 0;
		scd_.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd_.BufferDesc.RefreshRate.Numerator   = 60;
		scd_.BufferDesc.RefreshRate.Denominator = 1;
		scd_.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd_.OutputWindow                       = this->hWnd;
		scd_.SampleDesc.Count                   = 1;
		scd_.SampleDesc.Quality                 = 0;
		scd_.Windowed                           = TRUE;
		scd_.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
		scd_.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		UINT createFlags_ = 0;
#if defined(_DEBUG)
		// debug layer 는 SDK 미설치 환경에서 실패하므로 필수 아님.
		createFlags_ |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif
		const D3D_FEATURE_LEVEL aFeatureLevels_[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		D3D_FEATURE_LEVEL eFeatureLevel_{};
		const HRESULT hr_ = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createFlags_,
			aFeatureLevels_,
			static_cast<UINT>(sizeof(aFeatureLevels_) / sizeof(aFeatureLevels_[0])),
			D3D11_SDK_VERSION,
			&scd_,
			&this->pSwapChain,
			&this->pDevice,
			&eFeatureLevel_,
			&this->pContext);
		if (FAILED(hr_))
		{
			DBGPRINT(L"[ImGui] D3D11CreateDeviceAndSwapChain 실패 (0x%08X)", hr_);
			return;
		}
		CreateMainRTV_();
	}
	else
	{
		// 외부 device 주입 시 swapchain 도 외부에서 만들어 setter 로 받는 변형이
		// 필요하지만 현 호출자는 nullptr 만 넘기므로 이 경로는 미사용.
		this->pDevice = _pDevice;
		this->pDevice->AddRef();
		this->pDevice->GetImmediateContext(&this->pContext);
	}

	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::StyleColorsDark();

	// Docking / Multi-Viewport ConfigFlags 는 호출자(앱) 가 결정하도록 위임.
	// 라이브러리에서 강제 ON 하면 multi-viewport 비용을 모든 의존 프로젝트가
	// 떠안게 되므로 (secondary swapchain present 등 상시 비용),
	// 앱이 INI/메뉴 토글로 제어하게 둔다.

	ImGui_ImplWin32_Init(this->hWnd);
	ImGui_ImplDX11_Init(this->pDevice, this->pContext);

	// 기본 폰트
	const ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.Fonts->AddFontFromFileTTF("Fonts/DungGeunMo.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesKorean());

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_Right;
}

// ─────────────────────────────────────────────────────────────────────────────
// Update — 기본 메뉴 그리기. 실제 앱에서는 override 로 대체.
// ─────────────────────────────────────────────────────────────────────────────
long C_IMGUI::Update_ImGui()
{
	long nResult = 0;
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	do
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(this->v2MainWindowSize);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		flags |= ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;
		flags |= ImGuiWindowFlags_MenuBar;
		if (ImGui::Begin("MainMenu", nullptr, flags))
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu(toUtf8("파일")))
				{
					if (ImGui::MenuItem(toUtf8("종료"))) { nResult = -1; }
					ImGui::EndMenu();
				}
				ImGui::Text("[ImGui] FRAME (%.1f FPS)", ImGui::GetIO().Framerate);
				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
	} while (false);
	ImGui::EndFrame();
	return(nResult);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw — clear -> ImGui::Render -> RenderDrawData -> Multi-Viewport -> Present.
// ─────────────────────────────────────────────────────────────────────────────
void C_IMGUI::Draw_ImGui()
{
	if (this->bResetDevice) { return; }
	if (this->pContext == nullptr || this->pSwapChain == nullptr) { return; }
	if (this->pMainRTV == nullptr) { return; }

	// clear
	const float fClear_[4] = {
		static_cast<float>(this->dkClearColor.r)   / 255.0f,
		static_cast<float>(this->dkClearColor.g) / 255.0f,
		static_cast<float>(this->dkClearColor.b)  / 255.0f,
		1.0f,
	};
	this->pContext->OMSetRenderTargets(1, &this->pMainRTV, nullptr);
	this->pContext->ClearRenderTargetView(this->pMainRTV, fClear_);

	// ImGui 메인 viewport 렌더
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Multi-Viewport — secondary OS 창들 렌더 + Present (메인 viewport Present 전 호출).
	{
		const ImGuiIO& ioRender = ImGui::GetIO();
		if (ioRender.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	// Present (sync interval 1 = VSync ON, 0 = OFF)
	const UINT uSync_ = this->bVerticalSync ? 1u : 0u;
	const HRESULT hr_ = this->pSwapChain->Present(uSync_, 0);
	if (hr_ == DXGI_ERROR_DEVICE_REMOVED || hr_ == DXGI_ERROR_DEVICE_RESET)
	{
		// device lost → Resize 트리거. 실제 reset 은 ResetDevice 에서.
		this->bResetDevice = true;
		DBGPRINT(L"[ImGui] Present device lost (0x%08X) -> reset 예약", hr_);
	}
	else if (FAILED(hr_))
	{
		DBGPRINT(L"[ImGui] Present 실패 (0x%08X)", hr_);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Destroy — 역순 해제.
// ─────────────────────────────────────────────────────────────────────────────
void C_IMGUI::Destroy_ImGui()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	ReleaseMainRTV_();
	if (this->pSwapChain != nullptr) { this->pSwapChain->Release(); this->pSwapChain = nullptr; }
	if (this->pContext   != nullptr) { this->pContext->Release();   this->pContext   = nullptr; }
	if (this->pDevice    != nullptr) { this->pDevice->Release();    this->pDevice    = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
// ResetDevice — WM_SIZE 시 swapchain ResizeBuffers + RTV 재생성.
// (DX9 의 device Reset 과 등가. DX11 은 device 자체는 보존되고 backbuffer 만 재생성.)
// ─────────────────────────────────────────────────────────────────────────────
void C_IMGUI::ResetDevice()
{
	if (this->pSwapChain == nullptr) { return; }
	this->bResetDevice = true;

	// ImGui DX11 backend 의 device-bound 리소스(폰트 텍스처 등) 재생성 트리거.
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ReleaseMainRTV_();

	const UINT uW_ = static_cast<UINT>(this->v2MainWindowSize.x);
	const UINT uH_ = static_cast<UINT>(this->v2MainWindowSize.y);
	const HRESULT hr_ = this->pSwapChain->ResizeBuffers(0, uW_, uH_,
		DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	if (FAILED(hr_))
	{
		DBGPRINT(L"[ImGui] ResizeBuffers 실패 (0x%08X)", hr_);
	}

	CreateMainRTV_();
	ImGui_ImplDX11_CreateDeviceObjects();
	this->bResetDevice = false;
}

void C_IMGUI::SetWindowSize(UINT _x, UINT _y)
{
	this->v2MainWindowSize.x = static_cast<float>(_x);
	this->v2MainWindowSize.y = static_cast<float>(_y);
}

bool C_IMGUI::WndProc_ImGui(HWND _hWnd, UINT _nMessage, WPARAM _wParam, LPARAM _lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(_hWnd, _nMessage, _wParam, _lParam))
	{
		return(true);
	}
	return(false);
}

// =============================================================================
// 이하 — DX 백엔드와 무관한 헬퍼 (DX9 버전에서 그대로 가져옴).
// =============================================================================

typedef std::unordered_map<std::string, std::string> UMAP_UTF8_STRINGS;
UMAP_UTF8_STRINGS umapUtf8Strings;
LPCSTR toUtf8(LPCSTR _pszData)
{
	const UMAP_UTF8_STRINGS::iterator itr = umapUtf8Strings.find(_pszData);
	if (umapUtf8Strings.end() != itr)
	{
		return(itr->second.c_str());
	}
	else
	{
		umapUtf8Strings[_pszData] = dk::AnsiToUtf8(_pszData);
	}
	return(umapUtf8Strings[_pszData].c_str());
}

// 포인터 기반 캐시 (리터럴용 - O(1) 조회)
static thread_local std::unordered_map<const wchar_t*, std::string> g_umapPtrCache;
// 문자열 기반 캐시 (동적 문자열용)
static thread_local std::unordered_map<std::wstring, std::string> g_umapStrCache;

LPCSTR toUtf8(LPCWSTR _pwszData)
{
	if (!_pwszData || !*_pwszData) { return ""; }

	const auto itPtr = g_umapPtrCache.find(_pwszData);
	if (itPtr != g_umapPtrCache.end()) { return itPtr->second.c_str(); }

	const std::wstring strKey(_pwszData);
	const auto itStr = g_umapStrCache.find(strKey);
	if (itStr != g_umapStrCache.end())
	{
		g_umapPtrCache[_pwszData] = itStr->second;
		return itStr->second.c_str();
	}

	const std::string strUtf8 = dk::Utf16ToUtf8(_pwszData);
	g_umapStrCache[strKey] = strUtf8;
	g_umapPtrCache[_pwszData] = strUtf8;
	return g_umapStrCache[strKey].c_str();
}

bool bInit = false;
typedef std::unordered_map<std::string, ImVec4> UMAP_IMGUI_COLORS;
UMAP_IMGUI_COLORS umapImGuiColors;
void ColorInit()
{
}

LPCSTR GetColorName(size_t _nIndex)
{
	if (!bInit) { ColorInit(); bInit = true; }
	if (umapImGuiColors.size() > _nIndex)
	{
		size_t nIndex = 0;
		for (UMAP_IMGUI_COLORS::iterator itr = umapImGuiColors.begin(); umapImGuiColors.end() != itr; ++itr)
		{
			if (nIndex++ == _nIndex) { return itr->first.c_str(); }
		}
	}
	return(umapImGuiColors.begin()->first.c_str());
}

ImVec4 GetColorVec4OfText(LPCSTR _pColorName)
{
	if (!bInit) { ColorInit(); bInit = true; }
	if (_pColorName)
	{
		const UMAP_IMGUI_COLORS::iterator itr = umapImGuiColors.find(_pColorName);
		if (umapImGuiColors.end() != itr) { return(itr->second); }
	}
	return{ 0.0f, 0.0f, 0.0f, 0.0f };
}

ImU32 GetColorOfText(LPCSTR _pColorName)
{
	if (!bInit) { ColorInit(); bInit = true; }
	const UMAP_IMGUI_COLORS::iterator itr = umapImGuiColors.find(_pColorName);
	if (umapImGuiColors.end() != itr)
	{
		return(IM_COL32(itr->second.x, itr->second.y, itr->second.z, itr->second.w));
	}
	return(IM_COL32(0x00, 0x00, 0x00, 0x00));
}
