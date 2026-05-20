// DxWidget.h: GUI 위젯 추상 베이스.
// 위치/크기/이름/폰트스케일/visible 공통 속성 + Render(DrawContext&) virtual.
// 콘크리트: C_DX_LABEL / C_DX_BUTTON / C_DX_EDITBOX / C_DX_CHECKBOX.
// 컨테이너: C_DX_PAGE (vector<unique_ptr<C_DX_WIDGET>>).
#pragma once

#include "DxTypes.h"

#include <string>


namespace dx11
{

	class C_DX_DRAW_CONTEXT;


	enum E_DX_WIDGET_TYPE : uint8_t
	{
		DX_WIDGET_UNKNOWN  = 0,
		DX_WIDGET_LABEL    = 1,
		DX_WIDGET_BUTTON   = 2,
		DX_WIDGET_EDITBOX  = 3,
		DX_WIDGET_CHECKBOX = 4,
	};


	// 위젯 추상 베이스. 좌표는 부모(페이지) 원점 기준 픽셀.
	// 페이지가 매 프레임 Update(ctx) → Render(ctx) 호출.
	class C_DX_WIDGET
	{
	protected:
		std::string m_sKey;        // 영속 키 — m_GameLayout 등 외부 영속 시 식별자
		std::string m_sName;       // 표시명(라벨/버튼 텍스트). UTF-8.
		_DX_RECT    m_Rect;        // 위치/크기 (페이지 원점 기준 픽셀)
		float       m_fFontScale;  // 1.0 = 기본
		bool        m_bVisible;
		bool        m_bEnabled;

	public:
		C_DX_WIDGET()
			: m_fFontScale(1.0f)
			, m_bVisible(true)
			, m_bEnabled(true)
		{
		}
		virtual ~C_DX_WIDGET() = default;

		C_DX_WIDGET(const C_DX_WIDGET&) = delete;
		C_DX_WIDGET& operator=(const C_DX_WIDGET&) = delete;

		// ── 공통 속성 ──
		const std::string& GetKey()  const { return m_sKey; }
		void               SetKey(const std::string& _s) { m_sKey = _s; }

		const std::string& GetName() const { return m_sName; }
		void               SetName(const std::string& _s) { m_sName = _s; }

		const _DX_RECT& GetRect() const { return m_Rect; }
		void            SetRect(const _DX_RECT& _r) { m_Rect = _r; }
		void            SetPos(float _x, float _y) { m_Rect.x = _x; m_Rect.y = _y; }
		void            SetSize(float _w, float _h) { m_Rect.w = _w; m_Rect.h = _h; }

		float GetFontScale() const { return m_fFontScale; }
		void  SetFontScale(float _s) { m_fFontScale = _s; }

		bool IsVisible() const { return m_bVisible; }
		void SetVisible(bool _b) { m_bVisible = _b; }

		bool IsEnabled() const { return m_bEnabled; }
		void SetEnabled(bool _b) { m_bEnabled = _b; }

		// ── 타입 식별 (Serialize/팩토리 용) ──
		virtual E_DX_WIDGET_TYPE GetType() const = 0;
		virtual const char* GetTypeName() const = 0;   // "label" / "button" / ...

		// ── 렌더 / 입력 ──
		// 페이지가 _origin(절대 screen pos) 을 전달 — 위젯은 m_Rect 더해 절대 좌표.
		virtual void Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin) = 0;

		// 현재 프레임의 절대 사각형. 자식 위젯/hit-test 에서 사용.
		_DX_RECT AbsRect(_DX_POINT _origin) const
		{
			return _DX_RECT(_origin.x + m_Rect.x, _origin.y + m_Rect.y,
			                m_Rect.w, m_Rect.h);
		}
	};

} // namespace dx11
