// DX11Font.cpp: DirectWrite 기반 폰트 시스템 구현 (DX12Font 와 1:1 매핑).
#include "framework.h"
#include "DX11Font.h"
#include "DX11SpriteEngine.h"

#include <cstdarg>
#include <cstdio>
#include <vector>


namespace dx11
{
	//============================================================================
	// 정적 멤버
	//============================================================================
	Microsoft::WRL::ComPtr<IDWriteFactory3>          C_DX11_FONT::s_pFactory;
	Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1>   C_DX11_FONT::s_pSetBuilder;
	Microsoft::WRL::ComPtr<IDWriteFontCollection1>   C_DX11_FONT::s_pCustomCollection;
	bool                                              C_DX11_FONT::s_bCustomDirty = false;


	bool C_DX11_FONT::ensureFactory_()
	{
		if (s_pFactory) return true;

		IDWriteFactory* pBase_ = nullptr;
		HRESULT hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pBase_));
		if (FAILED(hr) || !pBase_)
		{
			DBGPRINT(L"[DX11Font] DWriteCreateFactory 실패: 0x%08X", hr);
			return false;
		}

		hr = pBase_->QueryInterface(IID_PPV_ARGS(s_pFactory.GetAddressOf()));
		pBase_->Release();
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] IDWriteFactory3 미지원: 0x%08X (Win10+ 필요)", hr);
			return false;
		}

		Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> pBaseBuilder_;
		hr = s_pFactory->CreateFontSetBuilder(pBaseBuilder_.GetAddressOf());
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] CreateFontSetBuilder 실패: 0x%08X", hr);
			s_pFactory.Reset();
			return false;
		}
		hr = pBaseBuilder_.As(&s_pSetBuilder);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] IDWriteFontSetBuilder1 미지원: 0x%08X (Win10 1709+)", hr);
			s_pFactory.Reset();
			return false;
		}
		return true;
	}


	bool C_DX11_FONT::ensureCustomCollection_()
	{
		if (!s_bCustomDirty && s_pCustomCollection) return true;
		if (!s_pFactory || !s_pSetBuilder) return false;

		Microsoft::WRL::ComPtr<IDWriteFontSet> pSet_;
		HRESULT hr = s_pSetBuilder->CreateFontSet(pSet_.GetAddressOf());
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] CreateFontSet 실패: 0x%08X", hr);
			return false;
		}

		s_pCustomCollection.Reset();
		hr = s_pFactory->CreateFontCollectionFromFontSet(pSet_.Get(),
			s_pCustomCollection.GetAddressOf());
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] CreateFontCollectionFromFontSet 실패: 0x%08X", hr);
			return false;
		}

		s_bCustomDirty = false;
		return true;
	}


	bool C_DX11_FONT::RegisterFontFile(const wchar_t* _pPath)
	{
		if (!_pPath || !*_pPath) return false;
		if (!ensureFactory_()) return false;

		Microsoft::WRL::ComPtr<IDWriteFontFile> pFile_;
		HRESULT hr = s_pFactory->CreateFontFileReference(_pPath, nullptr,
			pFile_.GetAddressOf());
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] CreateFontFileReference 실패: %ls (0x%08X)", _pPath, hr);
			return false;
		}

		BOOL bSupported_ = FALSE;
		DWRITE_FONT_FILE_TYPE eFileType_ = DWRITE_FONT_FILE_TYPE_UNKNOWN;
		DWRITE_FONT_FACE_TYPE eFaceType_ = DWRITE_FONT_FACE_TYPE_UNKNOWN;
		UINT32 uFaceCount_ = 0;
		hr = pFile_->Analyze(&bSupported_, &eFileType_, &eFaceType_, &uFaceCount_);
		if (FAILED(hr) || !bSupported_ || uFaceCount_ == 0)
		{
			DBGPRINT(L"[DX11Font] 폰트 파일 미지원: %ls", _pPath);
			return false;
		}

		hr = s_pSetBuilder->AddFontFile(pFile_.Get());
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Font] AddFontFile 실패: %ls (0x%08X)", _pPath, hr);
			return false;
		}

		s_bCustomDirty = true;
		DBGPRINT(L"[DX11Font] 폰트 등록: %ls (faces=%u)", _pPath, uFaceCount_);
		return true;
	}


	void C_DX11_FONT::CleanupStatic()
	{
		s_pCustomCollection.Reset();
		s_pSetBuilder.Reset();
		s_pFactory.Reset();
		s_bCustomDirty = false;
	}


	//============================================================================
	// 생성/소멸
	//============================================================================
	C_DX11_FONT::C_DX11_FONT()
		: m_fEmSize(0.0f)
		, m_fAscent(0.0f)
		, m_fDescent(0.0f)
		, m_fLineGap(0.0f)
		, m_uDesignUnitsPerEm(0)
		, m_hTexture(INVALID_TEXTURE)
		, m_uAtlasSize(0)
		, m_uShelfX(0)
		, m_uShelfY(0)
		, m_uShelfH(0)
		, m_bAtlasFull(false)
	{
	}


	C_DX11_FONT::~C_DX11_FONT()
	{
	}


	//============================================================================
	// 페이스 검색 (custom → system).
	//============================================================================
	static bool findFontFace_(IDWriteFactory3* _pFactory,
		IDWriteFontCollection1* _pCustom,
		const wchar_t* _pFaceName,
		bool _bBold, bool _bItalic,
		IDWriteFontFace** _ppOutFace)
	{
		const DWRITE_FONT_WEIGHT eWeight_ = _bBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
		const DWRITE_FONT_STYLE eStyle_ = _bItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
		const DWRITE_FONT_STRETCH eStretch_ = DWRITE_FONT_STRETCH_NORMAL;

		auto tryCollection_ = [&](IDWriteFontCollection* _pColl) -> bool
		{
			if (!_pColl) return false;
			UINT32 uIndex_ = 0;
			BOOL bExists_ = FALSE;
			HRESULT hr = _pColl->FindFamilyName(_pFaceName, &uIndex_, &bExists_);
			if (FAILED(hr) || !bExists_) return false;

			Microsoft::WRL::ComPtr<IDWriteFontFamily> pFamily_;
			hr = _pColl->GetFontFamily(uIndex_, pFamily_.GetAddressOf());
			if (FAILED(hr)) return false;

			Microsoft::WRL::ComPtr<IDWriteFont> pFont_;
			hr = pFamily_->GetFirstMatchingFont(eWeight_, eStretch_, eStyle_, pFont_.GetAddressOf());
			if (FAILED(hr)) return false;

			hr = pFont_->CreateFontFace(_ppOutFace);
			return SUCCEEDED(hr);
		};

		if (tryCollection_(_pCustom)) return true;

		Microsoft::WRL::ComPtr<IDWriteFontCollection> pSys_;
		if (FAILED(_pFactory->GetSystemFontCollection(pSys_.GetAddressOf(), FALSE)))
			return false;
		return tryCollection_(pSys_.Get());
	}


	//============================================================================
	// 초기화
	//============================================================================
	bool C_DX11_FONT::Initialize(C_DX11_SPRITE_ENGINE* _pEngine,
		const wchar_t* _pFaceName,
		uint32_t _uFontHeight,
		bool _bBold,
		bool _bItalic,
		uint32_t _uAtlasSize)
	{
		if (!_pEngine || !_pFaceName || _uFontHeight == 0 || _uAtlasSize == 0)
			return false;

		if (!ensureFactory_()) return false;
		ensureCustomCollection_();

		if (!findFontFace_(s_pFactory.Get(), s_pCustomCollection.Get(),
			_pFaceName, _bBold, _bItalic, m_pFace.GetAddressOf()))
		{
			DBGPRINT(L"[DX11Font] 폰트 페이스 찾을 수 없음: %ls (bold=%d italic=%d)",
				_pFaceName, _bBold ? 1 : 0, _bItalic ? 1 : 0);
			return false;
		}

		DWRITE_FONT_METRICS fm_{};
		m_pFace->GetMetrics(&fm_);
		m_uDesignUnitsPerEm = fm_.designUnitsPerEm;
		m_fEmSize = static_cast<float>(_uFontHeight);
		const float fUnitScale_ = m_fEmSize / static_cast<float>(fm_.designUnitsPerEm);
		m_fAscent = static_cast<float>(fm_.ascent) * fUnitScale_;
		m_fDescent = static_cast<float>(fm_.descent) * fUnitScale_;
		m_fLineGap = static_cast<float>(fm_.lineGap) * fUnitScale_;

		m_uAtlasSize = _uAtlasSize;
		m_hTexture = _pEngine->CreateDynamicTexture(m_uAtlasSize, m_uAtlasSize);
		if (m_hTexture == INVALID_TEXTURE)
		{
			DBGPRINT(L"[DX11Font] CreateDynamicTexture(%ux%u) 실패", m_uAtlasSize, m_uAtlasSize);
			m_pFace.Reset();
			return false;
		}

		m_uShelfX = 1;
		m_uShelfY = 1;
		m_uShelfH = 0;
		m_bAtlasFull = false;
		m_mapGlyphs.clear();
		m_mapGlyphs.reserve(512);

		DBGPRINT(L"[DX11Font] 초기화: %ls %ups %s%s atlas=%u ascent=%.1f descent=%.1f",
			_pFaceName, _uFontHeight,
			_bBold ? L"Bold " : L"",
			_bItalic ? L"Italic" : L"Regular",
			m_uAtlasSize, m_fAscent, m_fDescent);
		return true;
	}


	void C_DX11_FONT::Shutdown(C_DX11_SPRITE_ENGINE* _pEngine)
	{
		m_mapGlyphs.clear();
		m_pFace.Reset();

		if (_pEngine && m_hTexture != INVALID_TEXTURE)
		{
			_pEngine->ReleaseTexture(m_hTexture);
		}
		m_hTexture = INVALID_TEXTURE;
		m_uShelfX = m_uShelfY = m_uShelfH = 0;
		m_bAtlasFull = false;
	}


	//============================================================================
	// 글리프 추출
	//============================================================================
	const _DX11_FONT_GLYPH* C_DX11_FONT::getGlyph_(C_DX11_SPRITE_ENGINE* _pEngine, wchar_t _wChar)
	{
		const auto it = m_mapGlyphs.find(_wChar);
		if (it != m_mapGlyphs.end())
			return &it->second;
		if (!m_pFace) return nullptr;

		const UINT32 uCode_ = static_cast<UINT32>(_wChar);
		UINT16 uGlyphIdx_ = 0;
		HRESULT hr = m_pFace->GetGlyphIndicesW(&uCode_, 1, &uGlyphIdx_);
		if (FAILED(hr)) return nullptr;

		const float fUnitScale_ = m_fEmSize / static_cast<float>(m_uDesignUnitsPerEm);

		DWRITE_GLYPH_METRICS gm_{};
		hr = m_pFace->GetDesignGlyphMetrics(&uGlyphIdx_, 1, &gm_, FALSE);
		if (FAILED(hr)) return nullptr;
		const float fAdvance_ = static_cast<float>(gm_.advanceWidth) * fUnitScale_;

		FLOAT fGlyphAdvance_ = 0.0f;
		DWRITE_GLYPH_OFFSET offset_{};
		DWRITE_GLYPH_RUN run_{};
		run_.fontFace = m_pFace.Get();
		run_.fontEmSize = m_fEmSize;
		run_.glyphCount = 1;
		run_.glyphIndices = &uGlyphIdx_;
		run_.glyphAdvances = &fGlyphAdvance_;
		run_.glyphOffsets = &offset_;
		run_.isSideways = FALSE;
		run_.bidiLevel = 0;

		Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> pAnalysis_;
		hr = s_pFactory->CreateGlyphRunAnalysis(&run_, 1.0f, nullptr,
			DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
			DWRITE_MEASURING_MODE_NATURAL,
			0.0f, 0.0f,
			pAnalysis_.GetAddressOf());
		if (FAILED(hr) || !pAnalysis_) return nullptr;

		RECT bounds_{};
		hr = pAnalysis_->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds_);
		if (FAILED(hr)) return nullptr;

		const int32_t iBmpW_ = bounds_.right - bounds_.left;
		const int32_t iBmpH_ = bounds_.bottom - bounds_.top;

		_DX11_FONT_GLYPH glyph_{};
		glyph_.m_fAdvance = fAdvance_;

		if (iBmpW_ <= 0 || iBmpH_ <= 0)
		{
			const auto r_ = m_mapGlyphs.emplace(_wChar, glyph_);
			return &r_.first->second;
		}

		if (m_uShelfX + static_cast<uint32_t>(iBmpW_) + 1u > m_uAtlasSize)
		{
			m_uShelfY += m_uShelfH + 1u;
			m_uShelfX = 1;
			m_uShelfH = 0;
		}
		if (m_uShelfY + static_cast<uint32_t>(iBmpH_) + 1u > m_uAtlasSize)
		{
			m_bAtlasFull = true;
			const auto r_ = m_mapGlyphs.emplace(_wChar, glyph_);
			return &r_.first->second;
		}

		const uint32_t uAtlasX_ = m_uShelfX;
		const uint32_t uAtlasY_ = m_uShelfY;

		std::vector<uint8_t> vAlpha_(static_cast<size_t>(iBmpW_) * static_cast<size_t>(iBmpH_));
		hr = pAnalysis_->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds_,
			vAlpha_.data(), static_cast<UINT32>(vAlpha_.size()));
		if (FAILED(hr)) return nullptr;

		std::vector<uint32_t> vPixels_(vAlpha_.size());
		for (size_t i = 0; i < vAlpha_.size(); ++i)
		{
			vPixels_[i] = (static_cast<uint32_t>(vAlpha_[i]) << 24) | 0x00FFFFFFu;
		}

		_pEngine->UpdateTextureRegion(m_hTexture,
			uAtlasX_, uAtlasY_,
			static_cast<uint32_t>(iBmpW_), static_cast<uint32_t>(iBmpH_),
			vPixels_.data());

		m_uShelfX += static_cast<uint32_t>(iBmpW_) + 1u;
		if (static_cast<uint32_t>(iBmpH_) > m_uShelfH)
			m_uShelfH = static_cast<uint32_t>(iBmpH_);

		glyph_.m_sOffsetX = static_cast<int16_t>(bounds_.left);
		glyph_.m_sOffsetY = static_cast<int16_t>(bounds_.top);
		glyph_.m_uBmpWidth = static_cast<uint16_t>(iBmpW_);
		glyph_.m_uBmpHeight = static_cast<uint16_t>(iBmpH_);
		glyph_.m_uAtlasX = static_cast<uint16_t>(uAtlasX_);
		glyph_.m_uAtlasY = static_cast<uint16_t>(uAtlasY_);

		const auto r_ = m_mapGlyphs.emplace(_wChar, glyph_);
		return &r_.first->second;
	}


	//============================================================================
	// 텍스트 렌더링
	//============================================================================
	void C_DX11_FONT::RenderText(C_DX11_SPRITE_ENGINE* _pEngine,
		float _fX, float _fY, const wchar_t* _pText,
		uint32_t _uColor, float _fScale)
	{
		if (!_pEngine || !_pText) return;
		if (m_hTexture == INVALID_TEXTURE || !m_pFace) return;

		const float fAtlasInv_ = 1.0f / static_cast<float>(m_uAtlasSize);
		const float fBaselineY_ = _fY + m_fAscent * _fScale;
		float fPenX_ = _fX;

		for (const wchar_t* p = _pText; *p != L'\0'; ++p)
		{
			const wchar_t wc_ = *p;
			if (wc_ == L'\n' || wc_ == L'\r') continue;

			const _DX11_FONT_GLYPH* const pG_ = getGlyph_(_pEngine, wc_);
			if (!pG_) continue;

			if (pG_->m_uBmpWidth > 0 && pG_->m_uBmpHeight > 0)
			{
				const float fQX_ = fPenX_ + static_cast<float>(pG_->m_sOffsetX) * _fScale;
				const float fQY_ = fBaselineY_ + static_cast<float>(pG_->m_sOffsetY) * _fScale;
				const float fQW_ = static_cast<float>(pG_->m_uBmpWidth) * _fScale;
				const float fQH_ = static_cast<float>(pG_->m_uBmpHeight) * _fScale;
				const float fU0_ = static_cast<float>(pG_->m_uAtlasX) * fAtlasInv_;
				const float fV0_ = static_cast<float>(pG_->m_uAtlasY) * fAtlasInv_;
				const float fU1_ = static_cast<float>(pG_->m_uAtlasX + pG_->m_uBmpWidth) * fAtlasInv_;
				const float fV1_ = static_cast<float>(pG_->m_uAtlasY + pG_->m_uBmpHeight) * fAtlasInv_;

				_pEngine->Draw(m_hTexture,
					fQX_, fQY_, fQW_, fQH_,
					fU0_, fV0_, fU1_, fV1_,
					0.0f, _uColor);
			}

			fPenX_ += pG_->m_fAdvance * _fScale;
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


	float C_DX11_FONT::MeasureText(C_DX11_SPRITE_ENGINE* _pEngine,
		const wchar_t* _pText, float _fScale)
	{
		if (!_pEngine || !_pText) return 0.0f;
		if (m_hTexture == INVALID_TEXTURE || !m_pFace) return 0.0f;

		float fW_ = 0.0f;
		for (const wchar_t* p = _pText; *p != L'\0'; ++p)
		{
			const wchar_t wc_ = *p;
			if (wc_ == L'\n' || wc_ == L'\r') continue;
			const _DX11_FONT_GLYPH* const pG_ = getGlyph_(_pEngine, wc_);
			if (!pG_) continue;
			fW_ += pG_->m_fAdvance * _fScale;
		}
		return fW_;
	}

} // namespace dx11
