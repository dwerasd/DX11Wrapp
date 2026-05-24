// DxCheckbox.h: 체크박스 위젯. bool* 바인딩.
#pragma once

#include "DxWidget.h"
#include "DxDrawContext.h"

#include <functional>


namespace dx11
{

	class C_DX_CHECKBOX : public C_DX_WIDGET
	{
	private:
		bool*       m_pData;
		FontHandle  m_hFont;
		_DX_COLOR   m_TextColor;
		_DX_COLOR   m_BoxBgColor;
		_DX_COLOR   m_BoxCheckColor;
		_DX_COLOR   m_BoxBorderColor;
		float       m_fBoxSize;     // 박스 한 변 (픽셀)
		std::function<void(bool)> m_OnChange;   // 토글 직후 새 값으로 호출
		bool        m_bPressedInside{ false };  // LMB press 시 hit 안인지 — release 시 toggle 판정

	public:
		C_DX_CHECKBOX()
			: m_pData(nullptr)
			, m_hFont(INVALID_FONT)
			, m_TextColor(0xFF222838u)
			, m_BoxBgColor(0xFFFFFFFFu)
			, m_BoxCheckColor(0xFF236EE0u)
			, m_BoxBorderColor(0xFF1A2638u)   // 거의 검정에 가까운 진한 색
			, m_fBoxSize(18.0f)
		{
		}

		void Bind(bool* _p)              { m_pData = _p; }
		void SetFont(FontHandle _h)      { m_hFont = _h; }
		void SetTextColor(_DX_COLOR _c)  { m_TextColor = _c; }
		void SetBoxSize(float _s)        { m_fBoxSize = _s; }
		void SetOnChange(std::function<void(bool)> _fn) { m_OnChange = std::move(_fn); }

		// 타입
		E_DX_WIDGET_TYPE GetType() const override     { return DX_WIDGET_CHECKBOX; }
		const char*      GetTypeName() const override { return "checkbox"; }

		void Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin) override;
	};

} // namespace dx11
