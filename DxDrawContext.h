// DxDrawContext.h: GUI 렌더링 및 입력 조회 추상 인터페이스.
// 위젯은 본 인터페이스만 알고 백엔드(DX11/DX12/기타) 무관하게 동작.
// 구현체: C_DX_DRAW_CONTEXT_DX11 — C_DX11_SPRITE_ENGINE + C_DX11_FONT 활용.
//        C_DX_DRAW_CONTEXT_DX12 (향후) — DX12 백엔드.
#pragma once

#include "DxTypes.h"

#include <cstdint>


namespace dx11
{

	// 폰트 핸들 — RegisterFont 가 반환. -1=무효.
	using FontHandle = int32_t;
	inline constexpr FontHandle INVALID_FONT = -1;


	enum E_DX_TEXT_ALIGN : uint8_t
	{
		DX_TEXT_ALIGN_LEFT   = 0,
		DX_TEXT_ALIGN_CENTER = 1,
		DX_TEXT_ALIGN_RIGHT  = 2,
	};


	// 세로 정렬 — m_Rect.h 가 양수일 때만 의미. h<=0 이면 TOP 동작과 동일.
	enum E_DX_VTEXT_ALIGN : uint8_t
	{
		DX_VALIGN_TOP    = 0,
		DX_VALIGN_CENTER = 1,
		DX_VALIGN_BOTTOM = 2,
	};


	enum E_DX_MOUSE_BUTTON : uint8_t
	{
		DX_MOUSE_LEFT   = 0,
		DX_MOUSE_RIGHT  = 1,
		DX_MOUSE_MIDDLE = 2,
	};


	// 추상 DrawContext — 위젯은 본 인터페이스만 참조.
	class C_DX_DRAW_CONTEXT
	{
	public:
		virtual ~C_DX_DRAW_CONTEXT() = default;

		// ── 프레임 라이프사이클 ──
		// BeginFrame/EndFrame 사이에서 DrawText/FillRect 등 호출 가능.
		// 입력 큐(키/문자) 는 BeginFrame 시점에 클리어.
		virtual void BeginFrame() = 0;
		virtual void EndFrame()   = 0;

		// ── 폰트 등록 / 조회 ──
		// _pFace: 폰트 페이스("맑은 고딕" 등). _uPxHeight: 1.0 스케일 기준 픽셀 높이.
		// 반환: FontHandle (재사용 가능). 실패 시 INVALID_FONT.
		virtual FontHandle RegisterFont(const wchar_t* _pFace,
			uint32_t _uPxHeight, bool _bBold) = 0;

		// 텍스트 픽셀 너비/높이 측정 — hit-test/정렬용.
		virtual _DX_SIZE MeasureText(FontHandle _hFont,
			const wchar_t* _pText, float _fScale) = 0;

		// 폰트 한 줄 높이.
		virtual float GetFontHeight(FontHandle _hFont, float _fScale) = 0;

		// ── 텍스트 렌더 ──
		virtual void DrawText(FontHandle _hFont, _DX_POINT _pos,
			const wchar_t* _pText, _DX_COLOR _color, float _fScale) = 0;

		// ── 도형 ──
		virtual void FillRect(_DX_RECT _rect, _DX_COLOR _color) = 0;
		virtual void DrawRectOutline(_DX_RECT _rect, _DX_COLOR _color,
			float _fThickness) = 0;

		// ── 클립 영역 (자식 위젯 클리핑) ──
		virtual void PushClipRect(_DX_RECT _rect) = 0;
		virtual void PopClipRect() = 0;

		// ── 입력 조회 ──
		// 외부에서 매 프레임 UpdateInput 등으로 채워준 상태를 위젯이 조회.
		virtual _DX_POINT GetMousePos() const = 0;
		virtual bool IsMouseHovered(_DX_RECT _rect) const = 0;
		virtual bool IsMouseClicked(E_DX_MOUSE_BUTTON _btn) const = 0;
		virtual bool IsMouseDown(E_DX_MOUSE_BUTTON _btn) const = 0;
		virtual bool IsMouseReleased(E_DX_MOUSE_BUTTON _btn) const = 0;

		// Windows VK_* 코드. true = 이번 프레임에 키 다운.
		virtual bool IsKeyPressed(int _nVK) const = 0;

		// 이번 프레임에 들어온 텍스트 입력(IME 결과 포함). 없으면 nullptr.
		virtual const wchar_t* PollTextInput() const = 0;
	};

} // namespace dx11
