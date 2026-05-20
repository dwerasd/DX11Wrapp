// DxInput.h: DX11Wrapp 위젯용 자체 입력 상태.
// Win32 WndProc 가 제공 (mouse pos/button, key, WM_CHAR) → 위젯이 조회.
// ImGui io 의존 제거 — DxEditbox/Button/Checkbox 의 hit-test/focus/IME 자체 처리.
#pragma once

#include "DxTypes.h"

#include <cstdint>
#include <cstring>
#include <string>


namespace dx11
{

	// 입력 상태 — singleton (Input() 으로 접근).
	// 매 프레임 EndFrame 호출로 clicked/released/pressed/charQueue 클리어.
	// down/keyDown 은 상태 보존(WM_*UP 까지).
	struct _DX_INPUT_STATE
	{
		float fMouseX{ 0.0f };
		float fMouseY{ 0.0f };
		bool  bMouseDown    [3]   { false, false, false };
		bool  bMouseClicked [3]   { false, false, false };
		bool  bMouseReleased[3]   { false, false, false };
		bool  bKeyDown   [256]    {};
		bool  bKeyPressed[256]    {};
		std::wstring wsCharQueue;

		void EndFrame()
		{
			std::memset(bMouseClicked,  0, sizeof(bMouseClicked));
			std::memset(bMouseReleased, 0, sizeof(bMouseReleased));
			std::memset(bKeyPressed,    0, sizeof(bKeyPressed));
			wsCharQueue.clear();
		}

		// hit-test (절대 screen pixel 기준).
		bool IsMouseHovered(_DX_RECT _r) const
		{
			return fMouseX >= _r.x && fMouseX < _r.x + _r.w
			    && fMouseY >= _r.y && fMouseY < _r.y + _r.h;
		}
		bool IsMouseClicked (int _btn) const { return (_btn>=0 && _btn<3) ? bMouseClicked [_btn] : false; }
		bool IsMouseDown    (int _btn) const { return (_btn>=0 && _btn<3) ? bMouseDown    [_btn] : false; }
		bool IsMouseReleased(int _btn) const { return (_btn>=0 && _btn<3) ? bMouseReleased[_btn] : false; }
		bool IsKeyPressed(int _vk) const
		{
			const int idx_ = _vk & 0xFF;
			return bKeyPressed[idx_];
		}
		const wchar_t* PollTextInput() const
		{
			return wsCharQueue.empty() ? nullptr : wsCharQueue.c_str();
		}
	};

	// Singleton accessor (전역 1개).
	_DX_INPUT_STATE& Input();

} // namespace dx11
