// DxDrawContext_DX11.cpp: DX11 백엔드 구현.
#include "framework.h"
#include "DxDrawContext_DX11.h"
#include "DX11SpriteEngine.h"
#include "DX11Font.h"

#include <Windows.h>


namespace dx11
{

	C_DX_DRAW_CONTEXT_DX11::C_DX_DRAW_CONTEXT_DX11()
		: m_pEngine(nullptr)
		, m_MousePos(0.0f, 0.0f)
		, m_hWhiteTex(INVALID_TEXTURE)
	{
		m_bMouseDown[0] = m_bMouseDown[1] = m_bMouseDown[2] = false;
		m_bMousePrevDown[0] = m_bMousePrevDown[1] = m_bMousePrevDown[2] = false;
	}


	C_DX_DRAW_CONTEXT_DX11::~C_DX_DRAW_CONTEXT_DX11()
	{
		Shutdown();
	}


	bool C_DX_DRAW_CONTEXT_DX11::Initialize(C_DX11_SPRITE_ENGINE* _pEngine)
	{
		if (_pEngine == nullptr) { return false; }
		m_pEngine = _pEngine;

		// 흰색 1x1 텍스처 생성 (FillRect / DrawRectOutline 솔리드용).
		// ARGB 0xFFFFFFFF — 위젯 색조와 곱해져 최종 색상.
		const uint32_t uWhite_ = 0xFFFFFFFFu;
		m_hWhiteTex = m_pEngine->CreateTexture(&uWhite_, 1, 1);
		return m_hWhiteTex != INVALID_TEXTURE;
	}


	void C_DX_DRAW_CONTEXT_DX11::Shutdown()
	{
		for (auto* p_ : m_vFonts)
		{
			if (p_ != nullptr)
			{
				if (m_pEngine != nullptr) { p_->Shutdown(m_pEngine); }
				delete p_;
			}
		}
		m_vFonts.clear();
		m_vFontPxHeights.clear();

		if (m_pEngine != nullptr && m_hWhiteTex != INVALID_TEXTURE)
		{
			m_pEngine->ReleaseTexture(m_hWhiteTex);
			m_hWhiteTex = INVALID_TEXTURE;
		}
		m_pEngine = nullptr;
	}


	void C_DX_DRAW_CONTEXT_DX11::UpdateMouse(float _fX, float _fY,
		bool _bL, bool _bR, bool _bM)
	{
		m_bMousePrevDown[0] = m_bMouseDown[0];
		m_bMousePrevDown[1] = m_bMouseDown[1];
		m_bMousePrevDown[2] = m_bMouseDown[2];
		m_MousePos = _DX_POINT(_fX, _fY);
		m_bMouseDown[0] = _bL;
		m_bMouseDown[1] = _bR;
		m_bMouseDown[2] = _bM;
	}


	void C_DX_DRAW_CONTEXT_DX11::AppendTextInput(wchar_t _wch)
	{
		m_sTextInput.push_back(_wch);
	}


	void C_DX_DRAW_CONTEXT_DX11::BeginFrame()
	{
		// 텍스트 입력 큐 리셋 — 새 프레임에 들어온 문자만 누적.
		m_sTextInput.clear();
	}


	void C_DX_DRAW_CONTEXT_DX11::EndFrame()
	{
		// 본 컨텍스트는 단일 프레임 한정 상태만 유지.
		// 실제 BeginFrame/EndFrame on the C_DX11_SPRITE_ENGINE 은 외부 메인 루프 책임.
	}


	FontHandle C_DX_DRAW_CONTEXT_DX11::RegisterFont(const wchar_t* _pFace,
		uint32_t _uPxHeight, bool _bBold)
	{
		if (m_pEngine == nullptr) { return INVALID_FONT; }
		C_DX11_FONT* p_ = new C_DX11_FONT();
		if (!p_->Initialize(m_pEngine, _pFace, _uPxHeight, _bBold))
		{
			delete p_;
			return INVALID_FONT;
		}
		m_vFonts.push_back(p_);
		m_vFontPxHeights.push_back(_uPxHeight);
		return static_cast<FontHandle>(m_vFonts.size() - 1);
	}


	_DX_SIZE C_DX_DRAW_CONTEXT_DX11::MeasureText(FontHandle _hFont,
		const wchar_t* _pText, float _fScale)
	{
		if (_hFont < 0
			|| _hFont >= static_cast<FontHandle>(m_vFonts.size())
			|| m_vFonts[_hFont] == nullptr
			|| m_pEngine == nullptr)
		{
			return _DX_SIZE(0.0f, 0.0f);
		}
		const float fW_ = m_vFonts[_hFont]->MeasureText(m_pEngine, _pText, _fScale);
		const float fH_ = GetFontHeight(_hFont, _fScale);
		return _DX_SIZE(fW_, fH_);
	}


	float C_DX_DRAW_CONTEXT_DX11::GetFontHeight(FontHandle _hFont, float _fScale)
	{
		if (_hFont < 0 || _hFont >= static_cast<FontHandle>(m_vFontPxHeights.size()))
		{
			return 0.0f;
		}
		return static_cast<float>(m_vFontPxHeights[_hFont]) * _fScale;
	}


	void C_DX_DRAW_CONTEXT_DX11::DrawText(FontHandle _hFont, _DX_POINT _pos,
		const wchar_t* _pText, _DX_COLOR _color, float _fScale)
	{
		if (_hFont < 0
			|| _hFont >= static_cast<FontHandle>(m_vFonts.size())
			|| m_vFonts[_hFont] == nullptr
			|| m_pEngine == nullptr)
		{
			return;
		}
		m_vFonts[_hFont]->RenderText(m_pEngine,
			_pos.x, _pos.y, _pText, _color.argb, _fScale);
	}


	void C_DX_DRAW_CONTEXT_DX11::FillRect(_DX_RECT _rect, _DX_COLOR _color)
	{
		if (m_pEngine == nullptr || m_hWhiteTex == INVALID_TEXTURE) { return; }
		m_pEngine->Draw(m_hWhiteTex,
			_rect.x, _rect.y, _rect.w, _rect.h,
			0.0f, 0.0f, 1.0f, 1.0f,
			0.5f, _color.argb);
	}


	void C_DX_DRAW_CONTEXT_DX11::DrawRectOutline(_DX_RECT _rect,
		_DX_COLOR _color, float _fThickness)
	{
		const float t_ = (_fThickness < 1.0f) ? 1.0f : _fThickness;
		// 4 변을 FillRect 로 — 모서리 중복은 무시(픽셀 단위 차이).
		FillRect(_DX_RECT(_rect.x, _rect.y, _rect.w, t_), _color);                    // top
		FillRect(_DX_RECT(_rect.x, _rect.y + _rect.h - t_, _rect.w, t_), _color);     // bottom
		FillRect(_DX_RECT(_rect.x, _rect.y, t_, _rect.h), _color);                    // left
		FillRect(_DX_RECT(_rect.x + _rect.w - t_, _rect.y, t_, _rect.h), _color);     // right
	}


	void C_DX_DRAW_CONTEXT_DX11::PushClipRect(_DX_RECT _rect)
	{
		// TODO: 실제 GPU 시저 적용. 현재는 스택 보관만 (위젯 클리핑은 P3+ 에 도입 예정).
		m_vClipStack.push_back(_rect);
	}


	void C_DX_DRAW_CONTEXT_DX11::PopClipRect()
	{
		if (!m_vClipStack.empty()) { m_vClipStack.pop_back(); }
	}


	bool C_DX_DRAW_CONTEXT_DX11::IsKeyPressed(int _nVK) const
	{
		// 1차 구현 — GetAsyncKeyState 직접 폴링.
		// 후속: WndProc 이벤트 큐(키다운 이번 프레임 trigger) 로 정확도 향상 권장.
		return (::GetAsyncKeyState(_nVK) & 0x8000) != 0;
	}

} // namespace dx11
