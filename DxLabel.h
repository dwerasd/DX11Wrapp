// DxLabel.h: 텍스트 라벨 위젯.
// 색상 + 정렬(L/C/R) + 폰트 스케일. 클릭/포커스 없음.
#pragma once

#include "DxWidget.h"
#include "DxDrawContext.h"


namespace dx11
{

	class C_DX_LABEL : public C_DX_WIDGET
	{
	private:
		_DX_COLOR        m_Color;
		E_DX_TEXT_ALIGN  m_Align;
		FontHandle       m_hFont;   // 페이지가 등록한 폰트 핸들

	public:
		C_DX_LABEL()
			: m_Color(0xFF222838u)
			, m_Align(DX_TEXT_ALIGN_LEFT)
			, m_hFont(INVALID_FONT)
		{
		}

		// 속성 setter
		void SetColor(_DX_COLOR _c)            { m_Color = _c; }
		void SetAlign(E_DX_TEXT_ALIGN _a)      { m_Align = _a; }
		void SetFont(FontHandle _h)            { m_hFont = _h; }

		_DX_COLOR       GetColor() const       { return m_Color; }
		E_DX_TEXT_ALIGN GetAlign() const       { return m_Align; }
		FontHandle      GetFont()  const       { return m_hFont; }

		// 타입
		E_DX_WIDGET_TYPE GetType() const override     { return DX_WIDGET_LABEL; }
		const char*      GetTypeName() const override { return "label"; }

		// 렌더 — m_sName 을 UTF-8 → wide 변환 후 DrawText.
		// (P3 에서 페이지가 utf8→wide 변환 헬퍼 제공 권장. 지금은 인라인.)
		void Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin) override;
	};

} // namespace dx11
