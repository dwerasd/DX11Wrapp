// DxEditbox.cpp
#include "framework.h"
#include "DxEditbox.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>


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

	static std::string WToU8_(const std::wstring& _w)
	{
		if (_w.empty()) { return std::string(); }
		const int n_ = ::WideCharToMultiByte(CP_UTF8, 0,
			_w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n_ <= 0) { return std::string(); }
		std::string s_(static_cast<size_t>(n_) - 1, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, _w.c_str(), -1, &s_[0], n_,
			nullptr, nullptr);
		return s_;
	}


	std::string C_DX_EDITBOX::DataToString_() const
	{
		if (m_pData == nullptr) { return std::string(); }
		char buf_[64];
		switch (m_DataType)
		{
		case DX_EDIT_INT32:
			std::snprintf(buf_, sizeof(buf_), "%d",
				*static_cast<int32_t*>(m_pData));
			return std::string(buf_);
		case DX_EDIT_INT64:
			std::snprintf(buf_, sizeof(buf_), "%lld",
				static_cast<long long>(*static_cast<int64_t*>(m_pData)));
			return std::string(buf_);
		case DX_EDIT_FLOAT:
			std::snprintf(buf_, sizeof(buf_), "%g",
				*static_cast<float*>(m_pData));
			return std::string(buf_);
		case DX_EDIT_TEXT:
			return *static_cast<std::string*>(m_pData);
		case DX_EDIT_YMD:
		{
			const int32_t v_ = *static_cast<int32_t*>(m_pData);
			if (v_ <= 0) { return std::string(); }
			const int y_ = v_ / 10000;
			const int m_ = (v_ / 100) % 100;
			const int d_ = v_ % 100;
			std::snprintf(buf_, sizeof(buf_), "%04d-%02d-%02d", y_, m_, d_);
			return std::string(buf_);
		}
		}
		return std::string();
	}


	void C_DX_EDITBOX::StringToData_()
	{
		if (m_pData == nullptr) { return; }
		switch (m_DataType)
		{
		case DX_EDIT_INT32:
			*static_cast<int32_t*>(m_pData) =
				static_cast<int32_t>(std::strtol(m_sBuffer.c_str(), nullptr, 10));
			break;
		case DX_EDIT_INT64:
			*static_cast<int64_t*>(m_pData) =
				static_cast<int64_t>(std::strtoll(m_sBuffer.c_str(), nullptr, 10));
			break;
		case DX_EDIT_FLOAT:
			*static_cast<float*>(m_pData) =
				static_cast<float>(std::strtod(m_sBuffer.c_str(), nullptr));
			break;
		case DX_EDIT_TEXT:
			*static_cast<std::string*>(m_pData) = m_sBuffer;
			break;
		case DX_EDIT_YMD:
		{
			std::string digits_;
			for (char c_ : m_sBuffer)
			{
				if (c_ >= '0' && c_ <= '9') { digits_.push_back(c_); }
			}
			if (digits_.size() == 8)
			{
				*static_cast<int32_t*>(m_pData) =
					static_cast<int32_t>(std::strtol(digits_.c_str(), nullptr, 10));
			}
			// 불완전한 입력은 데이터 변경 안 함 (기존 값 보존).
			break;
		}
		}
	}


	// 숫자만 추출 후 "YYYY-MM-DD" 형식 재구성. 8자리 초과는 잘림.
	void C_DX_EDITBOX::NormalizeYmdBuffer_()
	{
		std::string digits_;
		for (char c_ : m_sBuffer)
		{
			if (c_ >= '0' && c_ <= '9') { digits_.push_back(c_); }
		}
		if (digits_.size() > 8) { digits_.resize(8); }
		std::string out_;
		for (size_t i_ = 0; i_ < digits_.size(); ++i_)
		{
			if (i_ == 4 || i_ == 6) { out_.push_back('-'); }
			out_.push_back(digits_[i_]);
		}
		m_sBuffer = std::move(out_);
		m_uCaret = m_sBuffer.size();
	}


	void C_DX_EDITBOX::Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin)
	{
		if (!m_bVisible) { return; }

		const _DX_RECT abs_ = AbsRect(_origin);
		const bool bHover_ = _ctx.IsMouseHovered(abs_);

		// 포커스 처리.
		if (_ctx.IsMouseClicked(DX_MOUSE_LEFT))
		{
			if (bHover_ && m_bEnabled)
			{
				if (!m_bFocused)
				{
					m_bFocused = true;
					m_sBuffer  = DataToString_();
					m_uCaret   = m_sBuffer.size();
				}
			}
			else if (m_bFocused)
			{
				StringToData_();
				m_bFocused = false;
			}
		}

		// 배경 + 테두리. hover/focused 색상 분기. focused 면 2px 두께.
		_ctx.FillRect(abs_, m_BgColor);
		const _DX_COLOR bdrCol_ = m_bFocused ? m_BorderFocusColor
		                       : (bHover_   ? m_BorderFocusColor : m_BorderColor);
		_ctx.DrawRectOutline(abs_, bdrCol_, m_bFocused ? 2.0f : 1.0f);

		// 표시 문자열 — 포커스 시 편집 버퍼, 아니면 바인딩 변수 값.
		const std::string sShow_ = m_bFocused ? m_sBuffer : DataToString_();
		if (!sShow_.empty() && m_hFont != INVALID_FONT)
		{
			const std::wstring sW_ = U8ToW_(sShow_);
			_ctx.DrawText(m_hFont,
				_DX_POINT(abs_.x + 4.0f, abs_.y + (abs_.h - _ctx.GetFontHeight(
					m_hFont, m_fFontScale)) * 0.5f),
				sW_.c_str(), m_TextColor, m_fFontScale);
		}

		// 키 입력 처리(포커스 시).
		if (m_bFocused && m_bEnabled)
		{
			// 백스페이스.
			if (_ctx.IsKeyPressed(VK_BACK) && !m_sBuffer.empty())
			{
				if (m_DataType == DX_EDIT_YMD)
				{
					// 숫자 한 자리 후퇴. dash 가 마지막이면 dash + 그 앞 숫자 함께 제거.
					m_sBuffer.pop_back();
					if (!m_sBuffer.empty() && m_sBuffer.back() == '-')
					{
						m_sBuffer.pop_back();
					}
					NormalizeYmdBuffer_();
				}
				else
				{
					// UTF-8 안전 — 마지막 코드포인트 제거 위해 연속 바이트(0x80~0xBF) 까지 후퇴.
					while (!m_sBuffer.empty())
					{
						const unsigned char c_ =
							static_cast<unsigned char>(m_sBuffer.back());
						m_sBuffer.pop_back();
						if ((c_ & 0xC0) != 0x80) { break; }
					}
					m_uCaret = m_sBuffer.size();
				}
			}
			// 텍스트 입력 큐 흡수 (IME 결과 포함).
			const wchar_t* pTxt_ = _ctx.PollTextInput();
			if (pTxt_ != nullptr)
			{
				const std::string sNew_ = WToU8_(std::wstring(pTxt_));
				if (m_DataType == DX_EDIT_TEXT)
				{
					if (m_sBuffer.size() + sNew_.size() <= m_uTextMax)
					{
						m_sBuffer += sNew_;
					}
					m_uCaret = m_sBuffer.size();
				}
				else if (m_DataType == DX_EDIT_YMD)
				{
					// 숫자만 받음 + dash 는 무시 (정규화가 자동 삽입).
					for (char c_ : sNew_)
					{
						if (c_ >= '0' && c_ <= '9') { m_sBuffer.push_back(c_); }
					}
					NormalizeYmdBuffer_();
				}
				else
				{
					// 숫자 모드 — 숫자/부호/소수점만 허용.
					for (char c_ : sNew_)
					{
						const bool bDigit_ = (c_ >= '0' && c_ <= '9');
						const bool bSign_  = (c_ == '-' && m_sBuffer.empty());
						const bool bDot_   = (c_ == '.' && m_DataType == DX_EDIT_FLOAT
							&& m_sBuffer.find('.') == std::string::npos);
						if (bDigit_ || bSign_ || bDot_) { m_sBuffer.push_back(c_); }
					}
					m_uCaret = m_sBuffer.size();
				}
			}
			// 엔터/Tab → 커밋 + 포커스 해제.
			if (_ctx.IsKeyPressed(VK_RETURN) || _ctx.IsKeyPressed(VK_TAB))
			{
				StringToData_();
				m_bFocused = false;
			}
		}
	}

} // namespace dx11
