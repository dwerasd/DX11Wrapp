// DxButton.cpp
#include "framework.h"
#include "DxButton.h"

#include <Windows.h>


namespace dx11
{

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


	void C_DX_BUTTON::Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin)
	{
		if (!m_bVisible) { return; }

		const _DX_RECT abs_ = AbsRect(_origin);
		const bool bHover_ = m_bEnabled && _ctx.IsMouseHovered(abs_);
		const bool bDown_  = bHover_ && _ctx.IsMouseDown(DX_MOUSE_LEFT);

		// ── 배경 (NORMAL 스타일만) ──
		if (m_Style == DX_BTN_STYLE_NORMAL)
		{
			const _DX_COLOR bg_ = !m_bEnabled ? _DX_COLOR(0xFF888888u)
				: (bDown_  ? m_BgPressedColor
				: (bHover_ ? m_BgHoverColor : m_BgColor));
			_ctx.FillRect(abs_, bg_);
			if (m_fBorderThickness > 0.0f)
			{
				_ctx.DrawRectOutline(abs_, m_BorderColor, m_fBorderThickness);
			}
		}

		// ── 텍스트 ──
		// 가로/세로 정렬은 m_Align / m_VAlign 으로 일반화.
		// (스타일별 default: NORMAL=CENTER/CENTER, MENU_TEXT=LEFT/CENTER — SetStyle 에서 세팅.)
		if (!m_sName.empty() && m_hFont != INVALID_FONT)
		{
			const std::wstring sW_ = U8ToW_(m_sName);
			const _DX_SIZE sz_ = _ctx.MeasureText(m_hFont, sW_.c_str(), m_fFontScale);
			float fTX_ = abs_.x;
			if (m_Align == DX_TEXT_ALIGN_CENTER) { fTX_ += (abs_.w - sz_.w) * 0.5f; }
			else if (m_Align == DX_TEXT_ALIGN_RIGHT) { fTX_ += (abs_.w - sz_.w); }
			float fTY_ = abs_.y;
			if (m_VAlign == DX_VALIGN_CENTER) { fTY_ += (abs_.h - sz_.h) * 0.5f; }
			else if (m_VAlign == DX_VALIGN_BOTTOM) { fTY_ += (abs_.h - sz_.h); }
			const _DX_COLOR col_ = bHover_ ? m_TextHoverColor : m_TextColor;
			_ctx.DrawText(m_hFont, _DX_POINT(fTX_, fTY_),
				sW_.c_str(), col_, m_fFontScale);
		}

		// ── 클릭 판정 ──
		if (m_bEnabled && bHover_ && _ctx.IsMouseReleased(DX_MOUSE_LEFT))
		{
			if (m_OnClick) { m_OnClick(); }
		}
	}

} // namespace dx11
