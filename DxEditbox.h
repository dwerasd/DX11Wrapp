// DxEditbox.h: 입력 박스 위젯.
// 타입: INT32 / INT64 / FLOAT / TEXT. 외부 변수 포인터에 직접 바인딩.
// 포커스 = 클릭 시 획득. 키 입력은 DrawContext::PollTextInput / IsKeyPressed.
// IME(한글) 한정 지원 — DrawContext 가 WM_CHAR/IME COMPOSITION 결과를
// AppendTextInput 으로 공급한다고 가정.
#pragma once

#include "DxWidget.h"
#include "DxDrawContext.h"


namespace dx11
{

	enum E_DX_EDIT_TYPE : uint8_t
	{
		DX_EDIT_INT32  = 0,
		DX_EDIT_INT64  = 1,
		DX_EDIT_FLOAT  = 2,
		DX_EDIT_TEXT   = 3,
		DX_EDIT_YMD    = 4,   // int32 YYYYMMDD 바인딩 — 표시는 YYYY-MM-DD 자동 마스킹
	};


	class C_DX_EDITBOX : public C_DX_WIDGET
	{
	private:
		E_DX_EDIT_TYPE m_DataType;
		void*          m_pData;          // 바인딩 변수 포인터 (타입 일치 책임=호출자)
		size_t         m_uTextMax;       // TEXT 타입의 max length (바이트, NUL 제외)
		FontHandle     m_hFont;
		_DX_COLOR      m_TextColor;
		_DX_COLOR      m_BgColor;
		_DX_COLOR      m_BorderColor;
		_DX_COLOR      m_BorderFocusColor;

		// 편집 상태
		bool        m_bFocused;
		std::string m_sBuffer;           // 편집 중 표시 버퍼 (TEXT) 또는 숫자 입력 버퍼
		size_t      m_uCaret;            // 캐럿 위치 (m_sBuffer 인덱스)
		bool        m_bSelected;         // focus 진입 직후 = true. 첫 입력/백스페이스 시 buffer 비움 + false.
		int         m_nBlinkCnt;         // caret blink 카운터 (frame 단위, 60 = ~1초 주기)

	public:
		C_DX_EDITBOX()
			: m_DataType(DX_EDIT_INT32)
			, m_pData(nullptr)
			, m_uTextMax(255)
			, m_hFont(INVALID_FONT)
			, m_TextColor(0xFF222838u)
			, m_BgColor(0xFFFFFFFFu)
			, m_BorderColor(0xFF8896A8u)
			, m_BorderFocusColor(0xFF236EE0u)
			, m_bFocused(false)
			, m_uCaret(0)
			, m_bSelected(false)
			, m_nBlinkCnt(0)
		{
		}

		// 바인딩 — 데이터 변수 포인터를 등록. 타입과 일치해야 함.
		void BindInt32(int32_t* _p)        { m_DataType = DX_EDIT_INT32; m_pData = _p; }
		void BindInt64(int64_t* _p)        { m_DataType = DX_EDIT_INT64; m_pData = _p; }
		void BindFloat(float* _p)          { m_DataType = DX_EDIT_FLOAT; m_pData = _p; }
		void BindText(std::string* _p, size_t _uMax = 255)
		{ m_DataType = DX_EDIT_TEXT; m_pData = _p; m_uTextMax = _uMax; }
		// YYYY-MM-DD 자동 마스킹. 내부는 int32 YYYYMMDD (예: 20200102).
		void BindYmd(int32_t* _p)          { m_DataType = DX_EDIT_YMD; m_pData = _p; }

		void SetFont(FontHandle _h)        { m_hFont = _h; }
		void SetTextColor(_DX_COLOR _c)    { m_TextColor = _c; }
		void SetBgColor(_DX_COLOR _c)      { m_BgColor = _c; }
		void SetBorderColor(_DX_COLOR _c, _DX_COLOR _focus)
		{ m_BorderColor = _c; m_BorderFocusColor = _focus; }

		E_DX_EDIT_TYPE GetDataType() const { return m_DataType; }
		bool IsFocused() const             { return m_bFocused; }
		bool IsSelected() const            { return m_bSelected; }
		size_t GetBufferSize() const       { return m_sBuffer.size(); }

		// 타입
		E_DX_WIDGET_TYPE GetType() const override     { return DX_WIDGET_EDITBOX; }
		const char*      GetTypeName() const override { return "editbox"; }

		void Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin) override;

	private:
		// 바인딩 변수 → 표시 문자열 (편집 시작/포커스 종료 시 동기).
		std::string DataToString_() const;
		// 표시 문자열 → 바인딩 변수 (편집 종료 / 엔터).
		void StringToData_();
		// YMD 버퍼 정규화 — 숫자만 추출 후 "YYYY-MM-DD" 형식 재구성.
		void NormalizeYmdBuffer_();
	};

} // namespace dx11
