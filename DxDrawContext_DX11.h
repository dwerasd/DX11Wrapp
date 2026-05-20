// DxDrawContext_DX11.h: DrawContext 의 DX11 백엔드 구현.
// C_DX11_SPRITE_ENGINE + C_DX11_FONT 를 활용 — 텍스트는 폰트 시스템의 GDI 글리프
// 아틀라스로 출력, 도형은 스프라이트 엔진의 색조 quad 로 출력.
// 외부에서 UpdateMouse / AppendTextInput / SetKeyDown 호출로 입력 상태 갱신.
#pragma once

#include "DxDrawContext.h"

#include <string>
#include <vector>


namespace dx11
{
	class C_DX11_SPRITE_ENGINE;
	class C_DX11_FONT;


	class C_DX_DRAW_CONTEXT_DX11 : public C_DX_DRAW_CONTEXT
	{
	private:
		C_DX11_SPRITE_ENGINE* m_pEngine;
		std::vector<C_DX11_FONT*> m_vFonts;             // FontHandle = vector 인덱스
		std::vector<uint32_t>     m_vFontPxHeights;     // 폰트별 1.0 스케일 픽셀 높이 (GetFontHeight 용)

		_DX_POINT m_MousePos;
		bool      m_bMouseDown[3];
		bool      m_bMousePrevDown[3];
		std::wstring m_sTextInput;                       // 이번 프레임 텍스트 입력 누적
		std::vector<_DX_RECT> m_vClipStack;             // 클립 스택 (현재 미적용 - 위치 보관만)

		// 솔리드 fill 용 흰색 1x1 텍스처 (FillRect/DrawRectOutline 에 사용).
		// Initialize 시 1회 생성.
		TextureHandle m_hWhiteTex;

	public:
		C_DX_DRAW_CONTEXT_DX11();
		~C_DX_DRAW_CONTEXT_DX11() override;

		C_DX_DRAW_CONTEXT_DX11(const C_DX_DRAW_CONTEXT_DX11&) = delete;
		C_DX_DRAW_CONTEXT_DX11& operator=(const C_DX_DRAW_CONTEXT_DX11&) = delete;

		// 엔진 attach — _pEngine 는 외부 소유. 솔리드 fill 용 흰색 1x1 텍스처 생성.
		bool Initialize(C_DX11_SPRITE_ENGINE* _pEngine);

		// 등록된 폰트 해제 + 흰색 텍스처 해제.
		void Shutdown();

		// ── 외부에서 매 프레임 입력 상태 갱신 (BeginFrame 전 호출) ──
		void UpdateMouse(float _fX, float _fY, bool _bL, bool _bR, bool _bM);
		void AppendTextInput(wchar_t _wch);

		// ── C_DX_DRAW_CONTEXT 오버라이드 ──
		void BeginFrame() override;
		void EndFrame()   override;

		FontHandle RegisterFont(const wchar_t* _pFace,
			uint32_t _uPxHeight, bool _bBold) override;
		_DX_SIZE MeasureText(FontHandle _hFont,
			const wchar_t* _pText, float _fScale) override;
		float GetFontHeight(FontHandle _hFont, float _fScale) override;

		void DrawText(FontHandle _hFont, _DX_POINT _pos,
			const wchar_t* _pText, _DX_COLOR _color, float _fScale) override;
		void FillRect(_DX_RECT _rect, _DX_COLOR _color) override;
		void DrawRectOutline(_DX_RECT _rect, _DX_COLOR _color,
			float _fThickness) override;

		void PushClipRect(_DX_RECT _rect) override;
		void PopClipRect() override;

		_DX_POINT GetMousePos() const override { return m_MousePos; }
		bool IsMouseHovered(_DX_RECT _rect) const override
		{
			return _rect.Contains(m_MousePos.x, m_MousePos.y);
		}
		bool IsMouseClicked(E_DX_MOUSE_BUTTON _btn) const override
		{
			const int n_ = static_cast<int>(_btn);
			return m_bMouseDown[n_] && !m_bMousePrevDown[n_];
		}
		bool IsMouseDown(E_DX_MOUSE_BUTTON _btn) const override
		{
			return m_bMouseDown[static_cast<int>(_btn)];
		}
		bool IsMouseReleased(E_DX_MOUSE_BUTTON _btn) const override
		{
			const int n_ = static_cast<int>(_btn);
			return !m_bMouseDown[n_] && m_bMousePrevDown[n_];
		}

		bool IsKeyPressed(int _nVK) const override;
		const wchar_t* PollTextInput() const override
		{
			return m_sTextInput.empty() ? nullptr : m_sTextInput.c_str();
		}
	};

} // namespace dx11
