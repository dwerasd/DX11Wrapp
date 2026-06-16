// DxgContextDX11.h: dxgui::IDrawContext 의 DX11 백엔드 구현(C_DRAW_CONTEXT_DX11).
// 위젯/차트가 D2D 와 동일한 추상으로 DX11 렌더 — 렌더러 무관 증명(P4 스캐폴드).
// 도형=C_DX11_SPRITE_ENGINE 의 축정렬 tinted quad(흰 1x1 텍스처) 분해, 텍스트=C_DX11_FONT.
//   선=축정렬 FillRect / 각진 선=DDA quad 스텝. 삼각형/quad=스캔라인 fill. 원=스캔라인/링 샘플.
//   clip=스택 + RSSetScissorRects(엔진 rasterizer ScissorEnable 시 실동작).
// 매크로(DrawText->DrawTextW) 일관성 위해 DX11Def.h(=windows.h) 를 DXGui 헤더보다 먼저 include.
#pragma once

#include "DX11Def.h"
#include <DXGui/DxgDrawContext.h>

#include <vector>
#include <string>


namespace dx11
{
	class C_DX11_SPRITE_ENGINE;
	class C_DX11_FONT;


	class C_DRAW_CONTEXT_DX11 : public dxgui::IDrawContext
	{
	private:
		C_DX11_SPRITE_ENGINE*      m_pEngine;	// 외부 소유
		std::vector<C_DX11_FONT*>  m_vFonts;	// FontHandle = 인덱스
		std::vector<uint32_t>      m_vFontPx;	// 폰트별 1.0 스케일 px 높이
		TextureHandle              m_hWhiteTex;	// 솔리드 fill 용 흰 1x1
		float                      m_fDepth;	// 스프라이트 깊이(0.5)

		// 입력 상태(폴링)
		dxgui::_DXG_POINT m_Mouse;
		bool m_bDown[3];
		bool m_bPrevDown[3];
		bool m_bKey[256];		// 에지(NewFrame 클리어)
		bool m_bKeyHeld[256];	// 레벨(KEYUP 까지 유지 — Ctrl/Shift)
		std::wstring m_sClipboard;	// GetClipboardText 반환 캐시
		std::wstring m_sTextInput;
		float m_fWheel;
		std::wstring m_sComposition;	// IME 조합중(호스트 설정)
		bool m_bCapture;		// 모달 입력 캡처
		int  m_nFrame;
		int  m_nLastLDownFrame;
		dxgui::_DXG_POINT m_LastLDownPos;
		bool m_bDblClick;
		std::vector<dxgui::_DXG_RECT> m_vClipStack;

		void rawFill_(float _x, float _y, float _w, float _h, uint32_t _argb);
		void fillTri_(const dxgui::_DXG_POINT& _a, const dxgui::_DXG_POINT& _b,
			const dxgui::_DXG_POINT& _c, uint32_t _argb);
		void applyScissor_();	// 스택 top(교집합)을 시저로 설정(없으면 해제)

	public:
		C_DRAW_CONTEXT_DX11();
		~C_DRAW_CONTEXT_DX11() override;

		C_DRAW_CONTEXT_DX11(const C_DRAW_CONTEXT_DX11&) = delete;
		C_DRAW_CONTEXT_DX11& operator=(const C_DRAW_CONTEXT_DX11&) = delete;

		// 엔진 attach + 흰 1x1 텍스처 생성.
		bool Initialize(C_DX11_SPRITE_ENGINE* _pEngine);
		void Shutdown();

		// ── 입력 주입(호스트) ──
		void SetMousePos(float _x, float _y) { m_Mouse.x = _x; m_Mouse.y = _y; }
		void SetMouseButton(dxgui::E_DXG_MOUSE_BUTTON _btn, bool _bDown);
		void SetKey(int _nVK, bool _bDown);
		void PushTextInput(const wchar_t* _pText);
		void AddWheel(float _fNotches) { m_fWheel += _fNotches; }
		void SetComposition(const wchar_t* _p) { m_sComposition = (_p != nullptr) ? _p : L""; }
		void NewFrame();	// prevDown=down, 키/텍스트/휠 클리어

		// ── dxgui::IDrawContext ──
		void BeginFrame() override {}
		void EndFrame() override {}

		dxgui::FontHandle RegisterFont(const wchar_t* _pFace, uint32_t _uPxHeight, bool _bBold) override;
		dxgui::_DXG_SIZE  MeasureText(dxgui::FontHandle _hFont, const wchar_t* _pText, float _fScale) override;
		float             GetFontHeight(dxgui::FontHandle _hFont, float _fScale) override;
		void DrawText(dxgui::FontHandle _hFont, dxgui::_DXG_POINT _pos,
			const wchar_t* _pText, dxgui::_DXG_COLOR _color, float _fScale) override;

		void FillRect(dxgui::_DXG_RECT _rect, dxgui::_DXG_COLOR _color) override;
		void DrawRectOutline(dxgui::_DXG_RECT _rect, dxgui::_DXG_COLOR _color, float _fThickness) override;
		void DrawLine(dxgui::_DXG_POINT _a, dxgui::_DXG_POINT _b, dxgui::_DXG_COLOR _color, float _fThickness) override;
		void PushClipRect(dxgui::_DXG_RECT _rect) override;
		void PopClipRect() override;

		void FillQuad(const dxgui::_DXG_POINT _pts[4], dxgui::_DXG_COLOR _color) override;
		void FillTriangle(const dxgui::_DXG_POINT _pts[3], dxgui::_DXG_COLOR _color) override;
		void FillCircle(dxgui::_DXG_POINT _c, float _fRadius, dxgui::_DXG_COLOR _color) override;
		void DrawCircle(dxgui::_DXG_POINT _c, float _fRadius, dxgui::_DXG_COLOR _color, float _fThickness) override;

		void SetInputCapture(bool _bCapture) override { m_bCapture = _bCapture; }

		dxgui::_DXG_POINT GetMousePos() const override { return m_Mouse; }
		bool IsMouseHovered(dxgui::_DXG_RECT _rect) const override
		{
			return !m_bCapture && _rect.Contains(m_Mouse.x, m_Mouse.y);
		}
		bool IsMouseClicked(dxgui::E_DXG_MOUSE_BUTTON _btn) const override;
		bool IsMouseDoubleClicked(dxgui::E_DXG_MOUSE_BUTTON _btn) const override
		{
			return !m_bCapture && (_btn == dxgui::DXG_MOUSE_LEFT) && m_bDblClick;
		}
		bool IsMouseDown(dxgui::E_DXG_MOUSE_BUTTON _btn) const override;
		bool IsMouseReleased(dxgui::E_DXG_MOUSE_BUTTON _btn) const override;
		bool IsKeyPressed(int _nVK) const override;
		bool IsKeyDown(int _nVK) const override
		{
			return (_nVK >= 0 && _nVK < 256) && m_bKeyHeld[_nVK];
		}
		void SetClipboardText(const wchar_t* _pText) override;
		const wchar_t* GetClipboardText() override;
		const wchar_t* PollTextInput() const override
		{
			return m_sTextInput.empty() ? nullptr : m_sTextInput.c_str();
		}
		float GetWheelDelta() const override { return m_bCapture ? 0.0f : m_fWheel; }
		const wchar_t* PollComposition() const override
		{
			return m_sComposition.empty() ? nullptr : m_sComposition.c_str();
		}
	};

} // namespace dx11
