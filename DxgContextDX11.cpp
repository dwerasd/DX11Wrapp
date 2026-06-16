// DxgContextDX11.cpp
#include "framework.h"
#include "DxgContextDX11.h"
#include "DX11SpriteEngine.h"
#include "DX11Device.h"
#include "DX11Font.h"

#include <cmath>
#include <algorithm>
#include <type_traits>


namespace dx11
{
	// 렌더러 무관 증명(P4) — DX11 백엔드가 dxgui::IDrawContext 의 전 순수가상을 구현(구상 클래스).
	// 위젯/차트가 소비하는 동일 추상을 D2D 와 DX11 양쪽에서 구현 가능함을 컴파일타임 보증.
	static_assert(!std::is_abstract<C_DRAW_CONTEXT_DX11>::value,
		"C_DRAW_CONTEXT_DX11 must fully implement dxgui::IDrawContext");

	C_DRAW_CONTEXT_DX11::C_DRAW_CONTEXT_DX11()
		: m_pEngine(nullptr)
		, m_hWhiteTex(INVALID_TEXTURE)
		, m_fDepth(0.5f)
		, m_Mouse(0.0f, 0.0f)
		, m_fWheel(0.0f)
	{
		for (int i = 0; i < 3; ++i) { m_bDown[i] = false; m_bPrevDown[i] = false; }
		for (int i = 0; i < 256; ++i) { m_bKey[i] = false; m_bKeyHeld[i] = false; }
		m_bCapture = false;
		m_nFrame = 0;
		m_nLastLDownFrame = -1000;
		m_LastLDownPos = dxgui::_DXG_POINT(0.0f, 0.0f);
		m_bDblClick = false;
	}

	C_DRAW_CONTEXT_DX11::~C_DRAW_CONTEXT_DX11()
	{
		Shutdown();
	}

	bool C_DRAW_CONTEXT_DX11::Initialize(C_DX11_SPRITE_ENGINE* _pEngine)
	{
		if (_pEngine == nullptr) { return false; }
		m_pEngine = _pEngine;
		const uint32_t uWhite = 0xFFFFFFFFu;	// tint 와 곱해져 최종 색
		m_hWhiteTex = m_pEngine->CreateTexture(&uWhite, 1, 1);
		return m_hWhiteTex != INVALID_TEXTURE;
	}

	void C_DRAW_CONTEXT_DX11::Shutdown()
	{
		for (C_DX11_FONT* p : m_vFonts)
		{
			if (p != nullptr)
			{
				if (m_pEngine != nullptr) { p->Shutdown(m_pEngine); }
				delete p;
			}
		}
		m_vFonts.clear();
		m_vFontPx.clear();
		if (m_pEngine != nullptr && m_hWhiteTex != INVALID_TEXTURE)
		{
			m_pEngine->ReleaseTexture(m_hWhiteTex);
			m_hWhiteTex = INVALID_TEXTURE;
		}
		m_pEngine = nullptr;
	}

	//------------------------------------------------------------------------------------------------
	// 입력 주입 / 프레임 경계
	//------------------------------------------------------------------------------------------------
	void C_DRAW_CONTEXT_DX11::SetMouseButton(dxgui::E_DXG_MOUSE_BUTTON _btn, bool _bDown)
	{
		const int i = static_cast<int>(_btn);
		if (i < 0 || i >= 3) { return; }
		if (i == 0 && _bDown)
		{
			const float dx = m_Mouse.x - m_LastLDownPos.x;
			const float dy = m_Mouse.y - m_LastLDownPos.y;
			m_bDblClick = (m_nFrame - m_nLastLDownFrame <= 30) && (dx * dx + dy * dy < 36.0f);
			m_nLastLDownFrame = m_nFrame;
			m_LastLDownPos = m_Mouse;
		}
		m_bDown[i] = _bDown;
	}

	void C_DRAW_CONTEXT_DX11::SetKey(int _nVK, bool _bDown)
	{
		if (_nVK >= 0 && _nVK < 256)
		{
			if (_bDown) { m_bKey[_nVK] = true; }	// 에지(NewFrame 클리어)
			m_bKeyHeld[_nVK] = _bDown;				// 레벨(KEYUP 까지 유지)
		}
	}

	// 클립보드 — CF_UNICODETEXT. D2D 백엔드와 동일 동작(렌더러 무관 호스트 서비스).
	void C_DRAW_CONTEXT_DX11::SetClipboardText(const wchar_t* _pText)
	{
		if (_pText == nullptr || !::OpenClipboard(nullptr)) { return; }
		::EmptyClipboard();
		const size_t nLen = ::wcslen(_pText);
		const HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, (nLen + 1) * sizeof(wchar_t));
		if (hMem != nullptr)
		{
			void* p = ::GlobalLock(hMem);
			if (p != nullptr)
			{
				::memcpy(p, _pText, (nLen + 1) * sizeof(wchar_t));
				::GlobalUnlock(hMem);
				::SetClipboardData(CF_UNICODETEXT, hMem);
			}
			else { ::GlobalFree(hMem); }
		}
		::CloseClipboard();
	}

	const wchar_t* C_DRAW_CONTEXT_DX11::GetClipboardText()
	{
		m_sClipboard.clear();
		if (!::IsClipboardFormatAvailable(CF_UNICODETEXT) || !::OpenClipboard(nullptr)) { return nullptr; }
		const HANDLE hData = ::GetClipboardData(CF_UNICODETEXT);
		if (hData != nullptr)
		{
			const wchar_t* p = static_cast<const wchar_t*>(::GlobalLock(hData));
			if (p != nullptr) { m_sClipboard = p; ::GlobalUnlock(hData); }
		}
		::CloseClipboard();
		return m_sClipboard.empty() ? nullptr : m_sClipboard.c_str();
	}

	void C_DRAW_CONTEXT_DX11::PushTextInput(const wchar_t* _pText)
	{
		if (_pText != nullptr) { m_sTextInput += _pText; }
	}

	void C_DRAW_CONTEXT_DX11::NewFrame()
	{
		for (int i = 0; i < 3; ++i) { m_bPrevDown[i] = m_bDown[i]; }
		for (int i = 0; i < 256; ++i) { m_bKey[i] = false; }
		m_sTextInput.clear();
		m_fWheel = 0.0f;
		++m_nFrame;
		m_bDblClick = false;
	}

	//------------------------------------------------------------------------------------------------
	// 내부 — 축정렬 quad / 스캔라인 / 시저
	//------------------------------------------------------------------------------------------------
	void C_DRAW_CONTEXT_DX11::rawFill_(float _x, float _y, float _w, float _h, uint32_t _argb)
	{
		if (m_pEngine == nullptr || m_hWhiteTex == INVALID_TEXTURE) { return; }
		if (_w <= 0.0f || _h <= 0.0f) { return; }
		m_pEngine->Draw(m_hWhiteTex, _x, _y, _w, _h, 0.0f, 0.0f, 1.0f, 1.0f, m_fDepth, _argb);
	}

	// 삼각형 스캔라인 fill — 정점 y 정렬 후 행별 수평 스팬을 quad 로.
	void C_DRAW_CONTEXT_DX11::fillTri_(const dxgui::_DXG_POINT& _a, const dxgui::_DXG_POINT& _b,
		const dxgui::_DXG_POINT& _c, uint32_t _argb)
	{
		dxgui::_DXG_POINT p0 = _a, p1 = _b, p2 = _c;
		if (p1.y < p0.y) { std::swap(p0, p1); }
		if (p2.y < p0.y) { std::swap(p0, p2); }
		if (p2.y < p1.y) { std::swap(p1, p2); }
		if (p2.y - p0.y < 0.5f) { return; }	// 퇴화(수평) — 무시

		const auto edgeX = [](const dxgui::_DXG_POINT& _pa, const dxgui::_DXG_POINT& _pb, float _fy) -> float
		{
			if (_pb.y - _pa.y == 0.0f) { return _pa.x; }
			const float t = (_fy - _pa.y) / (_pb.y - _pa.y);
			return _pa.x + t * (_pb.x - _pa.x);
		};

		const int yStart = static_cast<int>(std::floor(p0.y));
		const int yEnd   = static_cast<int>(std::ceil(p2.y));
		for (int y = yStart; y < yEnd; ++y)
		{
			const float fy = static_cast<float>(y) + 0.5f;
			if (fy < p0.y || fy > p2.y) { continue; }
			const float xLong = edgeX(p0, p2, fy);
			const float xShort = (fy < p1.y) ? edgeX(p0, p1, fy) : edgeX(p1, p2, fy);
			const float xL = (std::min)(xLong, xShort);
			const float xR = (std::max)(xLong, xShort);
			rawFill_(xL, static_cast<float>(y), xR - xL, 1.0f, _argb);
		}
	}

	void C_DRAW_CONTEXT_DX11::applyScissor_()
	{
		if (m_pEngine == nullptr || m_pEngine->GetDevice() == nullptr) { return; }
		ID3D11DeviceContext* pCtx = m_pEngine->GetDevice()->GetContext();
		if (pCtx == nullptr) { return; }
		// 스택 전체 교집합(없으면 전체 화면). rasterizer ScissorEnable 시 실제 클리핑.
		float l = 0.0f, t = 0.0f;
		float r = static_cast<float>(m_pEngine->GetScreenWidth());
		float b = static_cast<float>(m_pEngine->GetScreenHeight());
		for (const dxgui::_DXG_RECT& c : m_vClipStack)
		{
			l = (std::max)(l, c.x);
			t = (std::max)(t, c.y);
			r = (std::min)(r, c.x + c.w);
			b = (std::min)(b, c.y + c.h);
		}
		if (r < l) { r = l; }
		if (b < t) { b = t; }
		D3D11_RECT rc{ static_cast<LONG>(l), static_cast<LONG>(t), static_cast<LONG>(r), static_cast<LONG>(b) };
		pCtx->RSSetScissorRects(1, &rc);
	}

	//------------------------------------------------------------------------------------------------
	// 폰트 / 텍스트
	//------------------------------------------------------------------------------------------------
	dxgui::FontHandle C_DRAW_CONTEXT_DX11::RegisterFont(const wchar_t* _pFace, uint32_t _uPxHeight, bool _bBold)
	{
		if (m_pEngine == nullptr) { return dxgui::INVALID_FONT; }
		C_DX11_FONT* p = new C_DX11_FONT();
		if (!p->Initialize(m_pEngine, _pFace, _uPxHeight, _bBold))
		{
			delete p;
			return dxgui::INVALID_FONT;
		}
		m_vFonts.push_back(p);
		m_vFontPx.push_back(_uPxHeight);
		return static_cast<dxgui::FontHandle>(m_vFonts.size() - 1);
	}

	dxgui::_DXG_SIZE C_DRAW_CONTEXT_DX11::MeasureText(dxgui::FontHandle _hFont, const wchar_t* _pText, float _fScale)
	{
		if (_hFont < 0 || _hFont >= static_cast<dxgui::FontHandle>(m_vFonts.size())
			|| m_vFonts[_hFont] == nullptr || m_pEngine == nullptr)
		{
			return dxgui::_DXG_SIZE(0.0f, 0.0f);
		}
		const float fW = m_vFonts[_hFont]->MeasureText(m_pEngine, _pText, _fScale);
		return dxgui::_DXG_SIZE(fW, this->GetFontHeight(_hFont, _fScale));
	}

	float C_DRAW_CONTEXT_DX11::GetFontHeight(dxgui::FontHandle _hFont, float _fScale)
	{
		if (_hFont < 0 || _hFont >= static_cast<dxgui::FontHandle>(m_vFontPx.size())) { return 0.0f; }
		return static_cast<float>(m_vFontPx[_hFont]) * _fScale;
	}

	void C_DRAW_CONTEXT_DX11::DrawText(dxgui::FontHandle _hFont, dxgui::_DXG_POINT _pos,
		const wchar_t* _pText, dxgui::_DXG_COLOR _color, float _fScale)
	{
		if (_hFont < 0 || _hFont >= static_cast<dxgui::FontHandle>(m_vFonts.size())
			|| m_vFonts[_hFont] == nullptr || m_pEngine == nullptr || _pText == nullptr)
		{
			return;
		}
		m_vFonts[_hFont]->RenderText(m_pEngine, _pos.x, _pos.y, _pText, _color.argb, _fScale);
	}

	//------------------------------------------------------------------------------------------------
	// 도형
	//------------------------------------------------------------------------------------------------
	void C_DRAW_CONTEXT_DX11::FillRect(dxgui::_DXG_RECT _rect, dxgui::_DXG_COLOR _color)
	{
		rawFill_(_rect.x, _rect.y, _rect.w, _rect.h, _color.argb);
	}

	void C_DRAW_CONTEXT_DX11::DrawRectOutline(dxgui::_DXG_RECT _rect, dxgui::_DXG_COLOR _color, float _fThickness)
	{
		const float t = (_fThickness < 1.0f) ? 1.0f : _fThickness;
		const uint32_t c = _color.argb;
		rawFill_(_rect.x, _rect.y, _rect.w, t, c);							// top
		rawFill_(_rect.x, _rect.y + _rect.h - t, _rect.w, t, c);			// bottom
		rawFill_(_rect.x, _rect.y, t, _rect.h, c);							// left
		rawFill_(_rect.x + _rect.w - t, _rect.y, t, _rect.h, c);			// right
	}

	void C_DRAW_CONTEXT_DX11::DrawLine(dxgui::_DXG_POINT _a, dxgui::_DXG_POINT _b, dxgui::_DXG_COLOR _color, float _fThickness)
	{
		const float t = (_fThickness < 1.0f) ? 1.0f : _fThickness;
		const float dx = _b.x - _a.x;
		const float dy = _b.y - _a.y;
		const uint32_t c = _color.argb;
		if (std::fabs(dy) < 0.5f)			// 수평
		{
			rawFill_((std::min)(_a.x, _b.x), _a.y - t * 0.5f, std::fabs(dx), t, c);
		}
		else if (std::fabs(dx) < 0.5f)		// 수직
		{
			rawFill_(_a.x - t * 0.5f, (std::min)(_a.y, _b.y), t, std::fabs(dy), c);
		}
		else								// 각진 선 — DDA quad 스텝(축정렬 엔진 한계 근사)
		{
			const float len = std::sqrt(dx * dx + dy * dy);
			const int steps = static_cast<int>(std::ceil(len));
			for (int i = 0; i <= steps; ++i)
			{
				const float f = static_cast<float>(i) / static_cast<float>(steps);
				const float x = _a.x + dx * f;
				const float y = _a.y + dy * f;
				rawFill_(x - t * 0.5f, y - t * 0.5f, t, t, c);
			}
		}
	}

	void C_DRAW_CONTEXT_DX11::PushClipRect(dxgui::_DXG_RECT _rect)
	{
		m_vClipStack.push_back(_rect);
		this->applyScissor_();
	}

	void C_DRAW_CONTEXT_DX11::PopClipRect()
	{
		if (!m_vClipStack.empty()) { m_vClipStack.pop_back(); }
		this->applyScissor_();
	}

	void C_DRAW_CONTEXT_DX11::FillQuad(const dxgui::_DXG_POINT _pts[4], dxgui::_DXG_COLOR _color)
	{
		fillTri_(_pts[0], _pts[1], _pts[2], _color.argb);
		fillTri_(_pts[0], _pts[2], _pts[3], _color.argb);
	}

	void C_DRAW_CONTEXT_DX11::FillTriangle(const dxgui::_DXG_POINT _pts[3], dxgui::_DXG_COLOR _color)
	{
		fillTri_(_pts[0], _pts[1], _pts[2], _color.argb);
	}

	void C_DRAW_CONTEXT_DX11::FillCircle(dxgui::_DXG_POINT _c, float _fRadius, dxgui::_DXG_COLOR _color)
	{
		if (_fRadius < 0.5f) { return; }
		const int rr = static_cast<int>(std::ceil(_fRadius));
		for (int dy = -rr; dy <= rr; ++dy)
		{
			const float fdy = static_cast<float>(dy);
			const float v = _fRadius * _fRadius - fdy * fdy;
			if (v < 0.0f) { continue; }
			const float hw = std::sqrt(v);
			rawFill_(_c.x - hw, _c.y + fdy, hw * 2.0f, 1.0f, _color.argb);
		}
	}

	void C_DRAW_CONTEXT_DX11::DrawCircle(dxgui::_DXG_POINT _c, float _fRadius, dxgui::_DXG_COLOR _color, float _fThickness)
	{
		if (_fRadius < 0.5f) { return; }
		const float t = (_fThickness < 1.0f) ? 1.0f : _fThickness;
		int n = static_cast<int>(6.2831853f * _fRadius);
		if (n < 12) { n = 12; }
		for (int i = 0; i < n; ++i)
		{
			const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
			const float x = _c.x + _fRadius * std::cos(a);
			const float y = _c.y + _fRadius * std::sin(a);
			rawFill_(x - t * 0.5f, y - t * 0.5f, t, t, _color.argb);
		}
	}

	//------------------------------------------------------------------------------------------------
	// 입력 조회
	//------------------------------------------------------------------------------------------------
	bool C_DRAW_CONTEXT_DX11::IsMouseClicked(dxgui::E_DXG_MOUSE_BUTTON _btn) const
	{
		if (m_bCapture) { return false; }
		const int i = static_cast<int>(_btn);
		return (i >= 0 && i < 3) && !m_bPrevDown[i] && m_bDown[i];
	}

	bool C_DRAW_CONTEXT_DX11::IsMouseDown(dxgui::E_DXG_MOUSE_BUTTON _btn) const
	{
		if (m_bCapture) { return false; }
		const int i = static_cast<int>(_btn);
		return (i >= 0 && i < 3) && m_bDown[i];
	}

	bool C_DRAW_CONTEXT_DX11::IsMouseReleased(dxgui::E_DXG_MOUSE_BUTTON _btn) const
	{
		if (m_bCapture) { return false; }
		const int i = static_cast<int>(_btn);
		return (i >= 0 && i < 3) && m_bPrevDown[i] && !m_bDown[i];
	}

	bool C_DRAW_CONTEXT_DX11::IsKeyPressed(int _nVK) const
	{
		return (_nVK >= 0 && _nVK < 256) && m_bKey[_nVK];
	}

} // namespace dx11
