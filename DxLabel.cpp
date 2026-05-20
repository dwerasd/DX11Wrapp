// DxLabel.cpp
#include "framework.h"
#include "DxLabel.h"

#include <Windows.h>


namespace dx11
{

	// UTF-8 → wide. m_sName 은 UTF-8 저장(편집창 InputText 등).
	// 위젯 라이브러리 내부 헬퍼 — 호출자 외부 노출 안 함.
	static std::wstring U8ToW_(const std::string& _s)
	{
		if (_s.empty()) { return std::wstring(); }
		const int n_ = ::MultiByteToWideChar(CP_UTF8, 0,
			_s.c_str(), -1, nullptr, 0);
		if (n_ <= 0) { return std::wstring(); }
		std::wstring w_(static_cast<size_t>(n_) - 1, L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, _s.c_str(), -1, &w_[0], n_);
		return w_;
	}


	void C_DX_LABEL::Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin)
	{
		if (!m_bVisible || m_sName.empty() || m_hFont == INVALID_FONT)
		{
			return;
		}
		const std::wstring sW_ = U8ToW_(m_sName);
		const _DX_SIZE sz_ = _ctx.MeasureText(m_hFont, sW_.c_str(), m_fFontScale);

		float fX_ = _origin.x + m_Rect.x;
		const float fY_ = _origin.y + m_Rect.y;
		if (m_Rect.w > 0.0f)
		{
			if (m_Align == DX_TEXT_ALIGN_CENTER)
			{
				fX_ += (m_Rect.w - sz_.w) * 0.5f;
			}
			else if (m_Align == DX_TEXT_ALIGN_RIGHT)
			{
				fX_ += (m_Rect.w - sz_.w);
			}
		}
		_ctx.DrawText(m_hFont, _DX_POINT(fX_, fY_),
			sW_.c_str(), m_Color, m_fFontScale);
	}

} // namespace dx11
