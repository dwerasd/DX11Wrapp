// DX12Font.cpp: 동적 글리프 아틀라스 폰트 시스템 구현.
// 패턴 — RFont (on-demand GDI 글리프 → DX12 동적 텍스처 셀 업로드 → 스프라이트 quad tint).
#include "framework.h"
#include "DX11Font.h"
#include "DX11SpriteEngine.h"

#include <cstdarg>
#include <cstdio>
#include <vector>


namespace dx11
{

	//============================================================================
	// 생성/소멸
	//============================================================================
	C_DX11_FONT::C_DX11_FONT()
		: m_hTexture(INVALID_TEXTURE)
		, m_uAtlasSize(0)
		, m_uCellSize(0)
		, m_uCellsX(0)
		, m_uCellsY(0)
		, m_uCellCount(0)
		, m_uFontHeight(0)
		, m_hDC(nullptr)
		, m_hFont(nullptr)
		, m_hBitmap(nullptr)
		, m_pBits(nullptr)
		, m_uNextCell(0)
	{
	}


	C_DX11_FONT::~C_DX11_FONT()
	{
		// 텍스처는 SpriteEngine 소유 — Shutdown 에서 명시적으로 ReleaseTexture 필요.
		// 여기서는 GDI 만 안전망 정리.
		if (m_hFont)    { ::DeleteObject(m_hFont);    m_hFont = nullptr; }
		if (m_hBitmap)  { ::DeleteObject(m_hBitmap);  m_hBitmap = nullptr; }
		if (m_hDC)      { ::DeleteDC(m_hDC);          m_hDC = nullptr; }
		m_pBits = nullptr;
	}


	//============================================================================
	// 초기화
	//============================================================================
	bool C_DX11_FONT::Initialize(C_DX11_SPRITE_ENGINE* _pEngine,
		const wchar_t* _pFaceName,
		uint32_t _uFontHeight,
		bool _bBold,
		uint32_t _uCellSize,
		uint32_t _uAtlasSize)
	{
		if (!_pEngine || !_pFaceName || _uFontHeight == 0)
			return false;
		if (_uCellSize == 0 || _uAtlasSize == 0)
			return false;
		if (_uCellSize > _uAtlasSize || (_uAtlasSize % _uCellSize) != 0)
			return false;

		m_uAtlasSize = _uAtlasSize;
		m_uCellSize = _uCellSize;
		m_uCellsX = m_uAtlasSize / m_uCellSize;
		m_uCellsY = m_uAtlasSize / m_uCellSize;
		m_uCellCount = m_uCellsX * m_uCellsY;
		m_uFontHeight = _uFontHeight;

		// GDI DC + DIBSection (cell × cell, BGRA top-down).
		m_hDC = ::CreateCompatibleDC(nullptr);
		if (!m_hDC)
		{
			DBGPRINT(L"[DX11Font] CreateCompatibleDC 실패");
			return false;
		}

		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = static_cast<LONG>(m_uCellSize);
		bi.bmiHeader.biHeight = -static_cast<LONG>(m_uCellSize);	// top-down (음수)
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;

		void* pBits_ = nullptr;
		m_hBitmap = ::CreateDIBSection(m_hDC, &bi, DIB_RGB_COLORS, &pBits_, nullptr, 0);
		if (!m_hBitmap)
		{
			DBGPRINT(L"[DX11Font] CreateDIBSection 실패");
			::DeleteDC(m_hDC); m_hDC = nullptr;
			return false;
		}
		m_pBits = static_cast<uint32_t*>(pBits_);
		::SelectObject(m_hDC, m_hBitmap);

		// 폰트 생성 (lfHeight 음수 = 셀 높이 픽셀 단위 = 글리프 높이).
		m_hFont = ::CreateFontW(
			-static_cast<int>(m_uFontHeight), 0, 0, 0,
			_bBold ? FW_BOLD : FW_NORMAL,
			FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			_pFaceName);
		if (!m_hFont)
		{
			DBGPRINT(L"[DX11Font] CreateFontW 실패: face=%ls height=%u",
				_pFaceName, _uFontHeight);
			::DeleteObject(m_hBitmap); m_hBitmap = nullptr;
			::DeleteDC(m_hDC); m_hDC = nullptr;
			return false;
		}
		::SelectObject(m_hDC, m_hFont);

		// 글리프는 검정 배경 + 흰색 전경 (OPAQUE 모드로 셀 전체 검정 채움 → R 채널 → 알파).
		::SetTextColor(m_hDC, RGB(255, 255, 255));
		::SetBkColor(m_hDC, RGB(0, 0, 0));
		::SetBkMode(m_hDC, OPAQUE);

		// 동적 텍스처 (BeginFrame 외부 호출 — 일회용 cmdlist 로 0 초기화 + PSR 전환).
		m_hTexture = _pEngine->CreateDynamicTexture(m_uAtlasSize, m_uAtlasSize);
		if (m_hTexture == INVALID_TEXTURE)
		{
			DBGPRINT(L"[DX11Font] CreateDynamicTexture(%ux%u) 실패",
				m_uAtlasSize, m_uAtlasSize);
			::DeleteObject(m_hFont); m_hFont = nullptr;
			::DeleteObject(m_hBitmap); m_hBitmap = nullptr;
			::DeleteDC(m_hDC); m_hDC = nullptr;
			return false;
		}

		m_uNextCell = 0;
		m_mapGlyphs.clear();
		m_mapGlyphs.reserve(m_uCellCount);

		DBGPRINT(L"[DX11Font] 초기화: %ux%u atlas, %ux%u cell, %u slots, %ls %ups %s",
			m_uAtlasSize, m_uAtlasSize, m_uCellSize, m_uCellSize, m_uCellCount,
			_pFaceName, _uFontHeight, _bBold ? L"Bold" : L"Regular");
		return true;
	}


	//============================================================================
	// 해제
	//============================================================================
	void C_DX11_FONT::Shutdown(C_DX11_SPRITE_ENGINE* _pEngine)
	{
		m_mapGlyphs.clear();
		m_uNextCell = 0;

		if (m_hFont)    { ::DeleteObject(m_hFont);    m_hFont = nullptr; }
		if (m_hBitmap)  { ::DeleteObject(m_hBitmap);  m_hBitmap = nullptr; }
		if (m_hDC)      { ::DeleteDC(m_hDC);          m_hDC = nullptr; }
		m_pBits = nullptr;

		if (_pEngine && m_hTexture != INVALID_TEXTURE)
		{
			_pEngine->ReleaseTexture(m_hTexture);
			m_hTexture = INVALID_TEXTURE;
		}
	}


	//============================================================================
	// 글리프 캐시 (on-demand GDI → 텍스처 셀 업로드)
	//============================================================================
	const _DX11_FONT_GLYPH* C_DX11_FONT::getGlyph_(C_DX11_SPRITE_ENGINE* _pEngine, wchar_t _wChar)
	{
		// 캐시 hit.
		const auto it = m_mapGlyphs.find(_wChar);
		if (it != m_mapGlyphs.end())
			return &it->second;

		// 아틀라스 풀 — 더 이상 등록 불가.
		if (m_uNextCell >= m_uCellCount)
			return nullptr;

		// GDI 렌더 — OPAQUE + ETO_OPAQUE 로 셀 배경 검정 채움 + 글리프 흰색.
		const RECT rcCell_ = { 0, 0,
			static_cast<LONG>(m_uCellSize), static_cast<LONG>(m_uCellSize) };
		::ExtTextOutW(m_hDC, 1, 1, ETO_OPAQUE | ETO_CLIPPED,
			&rcCell_, &_wChar, 1, nullptr);

		// 글리프 advance 너비 측정.
		SIZE sz_{};
		::GetTextExtentPoint32W(m_hDC, &_wChar, 1, &sz_);

		// BGRA (DIBSection) → R8G8B8A8: Red 채널 → Alpha (흑백 글리프 = 알파 마스크).
		// 셀 픽셀 수 = m_uCellSize^2. 큰 셀(64+) 도 안전하게 heap 사용.
		const uint32_t uPixels_ = m_uCellSize * m_uCellSize;
		std::vector<uint32_t> vCell_(uPixels_);
		for (uint32_t i = 0; i < uPixels_; ++i)
		{
			const uint32_t uGDI_ = m_pBits[i];
			const uint8_t uRed_ = static_cast<uint8_t>((uGDI_ >> 16) & 0xFFu);
			vCell_[i] = (static_cast<uint32_t>(uRed_) << 24) | 0x00FFFFFFu;
		}

		// 다음 빈 셀에 업로드.
		const uint32_t uCellX_ = m_uNextCell % m_uCellsX;
		const uint32_t uCellY_ = m_uNextCell / m_uCellsX;

		_pEngine->UpdateTextureRegion(m_hTexture,
			uCellX_ * m_uCellSize, uCellY_ * m_uCellSize,
			m_uCellSize, m_uCellSize, vCell_.data());

		// 캐시 등록.
		_DX11_FONT_GLYPH glyph_{};
		glyph_.m_uCellIndex = static_cast<uint16_t>(m_uNextCell);
		glyph_.m_uWidth = static_cast<uint8_t>(sz_.cx > 255 ? 255 : sz_.cx);
		glyph_.m_uHeight = static_cast<uint8_t>(sz_.cy > 255 ? 255 : sz_.cy);

		const auto result_ = m_mapGlyphs.emplace(_wChar, glyph_);
		++m_uNextCell;
		return &result_.first->second;
	}


	//============================================================================
	// 텍스트 렌더링
	//============================================================================
	void C_DX11_FONT::RenderText(C_DX11_SPRITE_ENGINE* _pEngine,
		float _fX, float _fY, const wchar_t* _pText,
		uint32_t _uColor, float _fScale)
	{
		if (!_pEngine || !_pText) return;
		if (m_hTexture == INVALID_TEXTURE) return;

		const float fTexInv_ = 1.0f / static_cast<float>(m_uAtlasSize);
		const float fQuadSize_ = static_cast<float>(m_uCellSize) * _fScale;
		const float fSpaceAdvance_ = static_cast<float>(m_uFontHeight) * 0.4f * _fScale;
		const float fFallbackAdvance_ = static_cast<float>(m_uFontHeight) * 0.5f * _fScale;

		float fCurX_ = _fX;

		for (const wchar_t* p = _pText; *p != L'\0'; ++p)
		{
			const wchar_t wc_ = *p;

			// 줄바꿈/캐리지리턴 무시 (단일 라인 렌더 — 멀티라인은 호출자가 분리).
			if (wc_ == L'\n' || wc_ == L'\r')
				continue;

			// 스페이스: 폰트 높이 × 0.4 전진.
			if (wc_ == L' ')
			{
				fCurX_ += fSpaceAdvance_;
				continue;
			}

			// on-demand 글리프 조회/생성.
			const _DX11_FONT_GLYPH* const pGlyph_ = getGlyph_(_pEngine, wc_);
			if (!pGlyph_)
			{
				fCurX_ += fFallbackAdvance_;
				continue;
			}

			// UV 계산 (셀 인덱스 → 아틀라스 좌표).
			const uint32_t uCellX_ = pGlyph_->m_uCellIndex % m_uCellsX;
			const uint32_t uCellY_ = pGlyph_->m_uCellIndex / m_uCellsX;
			const float fU0_ = static_cast<float>(uCellX_ * m_uCellSize) * fTexInv_;
			const float fV0_ = static_cast<float>(uCellY_ * m_uCellSize) * fTexInv_;
			const float fU1_ = fU0_ + static_cast<float>(m_uCellSize) * fTexInv_;
			const float fV1_ = fV0_ + static_cast<float>(m_uCellSize) * fTexInv_;

			_pEngine->Draw(m_hTexture,
				fCurX_, _fY, fQuadSize_, fQuadSize_,
				fU0_, fV0_, fU1_, fV1_,
				0.0f, _uColor);

			fCurX_ += static_cast<float>(pGlyph_->m_uWidth) * _fScale;
		}
	}


	void C_DX11_FONT::RenderTextFmt(C_DX11_SPRITE_ENGINE* _pEngine,
		float _fX, float _fY, uint32_t _uColor, float _fScale,
		const wchar_t* _pFmt, ...)
	{
		if (!_pEngine || !_pFmt) return;

		wchar_t szBuf_[512];
		va_list ap_;
		va_start(ap_, _pFmt);
		::vswprintf_s(szBuf_, _pFmt, ap_);
		va_end(ap_);

		RenderText(_pEngine, _fX, _fY, szBuf_, _uColor, _fScale);
	}


	//============================================================================
	// 너비 측정 (hit-test / 정렬)
	//============================================================================
	float C_DX11_FONT::MeasureText(C_DX11_SPRITE_ENGINE* _pEngine,
		const wchar_t* _pText, float _fScale)
	{
		if (!_pEngine || !_pText) return 0.0f;
		if (m_hTexture == INVALID_TEXTURE) return 0.0f;

		const float fSpaceAdvance_ = static_cast<float>(m_uFontHeight) * 0.4f * _fScale;
		const float fFallbackAdvance_ = static_cast<float>(m_uFontHeight) * 0.5f * _fScale;

		float fWidth_ = 0.0f;
		for (const wchar_t* p = _pText; *p != L'\0'; ++p)
		{
			const wchar_t wc_ = *p;
			if (wc_ == L'\n' || wc_ == L'\r') continue;
			if (wc_ == L' ') { fWidth_ += fSpaceAdvance_; continue; }

			const _DX11_FONT_GLYPH* const pGlyph_ = getGlyph_(_pEngine, wc_);
			if (!pGlyph_) { fWidth_ += fFallbackAdvance_; continue; }
			fWidth_ += static_cast<float>(pGlyph_->m_uWidth) * _fScale;
		}
		return fWidth_;
	}

} // namespace dx11
