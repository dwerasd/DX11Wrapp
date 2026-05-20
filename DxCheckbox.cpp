// DxCheckbox.cpp
#include "framework.h"
#include "DxCheckbox.h"

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


	void C_DX_CHECKBOX::Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin)
	{
		if (!m_bVisible) { return; }

		const _DX_RECT abs_ = AbsRect(_origin);
		const _DX_RECT box_(abs_.x, abs_.y + (abs_.h - m_fBoxSize) * 0.5f,
			m_fBoxSize, m_fBoxSize);

		// 박스 — 배경 + 테두리.
		_ctx.FillRect(box_, m_BoxBgColor);
		_ctx.DrawRectOutline(box_, m_BoxBorderColor, 1.0f);

		// 체크 표시 — 박스 내부 작은 사각형으로 단순화 (체크 마크 대신).
		if (m_pData != nullptr && *m_pData)
		{
			const float pad_ = m_fBoxSize * 0.2f;
			_ctx.FillRect(_DX_RECT(box_.x + pad_, box_.y + pad_,
				m_fBoxSize - pad_ * 2.0f, m_fBoxSize - pad_ * 2.0f),
				m_BoxCheckColor);
		}

		// 라벨 텍스트 — 박스 우측.
		if (!m_sName.empty() && m_hFont != INVALID_FONT)
		{
			const std::wstring sW_ = U8ToW_(m_sName);
			const float fH_ = _ctx.GetFontHeight(m_hFont, m_fFontScale);
			_ctx.DrawText(m_hFont,
				_DX_POINT(box_.x + m_fBoxSize + 6.0f,
					abs_.y + (abs_.h - fH_) * 0.5f),
				sW_.c_str(), m_TextColor, m_fFontScale);
		}

		// 클릭 — 박스 또는 라벨 영역 클릭 시 토글.
		if (m_bEnabled && m_pData != nullptr
			&& _ctx.IsMouseHovered(abs_)
			&& _ctx.IsMouseReleased(DX_MOUSE_LEFT))
		{
			*m_pData = !(*m_pData);
		}
	}

} // namespace dx11
