// DX11Font.h: DirectWrite 기반 고품질 폰트 시스템 (DX12Font 와 1:1 매핑).
// 글리프 추출 — IDWriteFontFace + CreateGlyphRunAnalysis (sub-pixel positioning, OpenType,
// Variable Font 지원). 알파 비트맵 → R8G8B8A8 변환 → 동적 atlas shelf-packing → quad tint 출력.
// TTF 파일 process-private 등록 지원 (다른 앱에 노출 X).
#pragma once

#include "DX11Def.h"

#include <dwrite_3.h>

#include <unordered_map>

#pragma comment(lib, "dwrite.lib")


namespace dx11
{
	class C_DX11_SPRITE_ENGINE;


	// 글리프 메타 — bearing/advance 보관 (DirectWrite 정밀 메트릭).
	struct _DX11_FONT_GLYPH
	{
		int16_t m_sOffsetX;		// pen X 기준 비트맵 left (글리프 left bearing, 음수 가능)
		int16_t m_sOffsetY;		// baseline 기준 비트맵 top (DirectWrite bounds.top, 음수 = baseline 위)
		uint16_t m_uBmpWidth;	// 비트맵 픽셀 너비
		uint16_t m_uBmpHeight;
		uint16_t m_uAtlasX;		// atlas 내 좌상단
		uint16_t m_uAtlasY;
		float m_fAdvance;		// sub-pixel 정밀 advance
	};


	class C_DX11_FONT
	{
	private:
		static constexpr uint32_t DEFAULT_ATLAS_SIZE = 1024;

		// DirectWrite 공유 자원.
		static Microsoft::WRL::ComPtr<IDWriteFactory3> s_pFactory;
		static Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> s_pSetBuilder;
		static Microsoft::WRL::ComPtr<IDWriteFontCollection1> s_pCustomCollection;
		static bool s_bCustomDirty;

		static bool ensureFactory_();
		static bool ensureCustomCollection_();

		// 폰트 페이스 + 메트릭.
		Microsoft::WRL::ComPtr<IDWriteFontFace> m_pFace;
		float m_fEmSize;
		float m_fAscent;
		float m_fDescent;
		float m_fLineGap;
		uint32_t m_uDesignUnitsPerEm;

		// Atlas + shelf packing.
		TextureHandle m_hTexture;
		uint32_t m_uAtlasSize;
		uint32_t m_uShelfX;
		uint32_t m_uShelfY;
		uint32_t m_uShelfH;
		bool m_bAtlasFull;

		std::unordered_map<wchar_t, _DX11_FONT_GLYPH> m_mapGlyphs;

		const _DX11_FONT_GLYPH* getGlyph_(C_DX11_SPRITE_ENGINE* _pEngine, wchar_t _wChar);

	public:
		C_DX11_FONT();
		~C_DX11_FONT();
		C_DX11_FONT(const C_DX11_FONT&) = delete;
		C_DX11_FONT& operator=(const C_DX11_FONT&) = delete;

		// TTF 파일 등록 (process-private). Initialize 호출 전 권장.
		static bool RegisterFontFile(const wchar_t* _pPath);

		// 정적 자원 해제 (앱 종료 시, 모든 인스턴스 Shutdown 이후).
		static void CleanupStatic();

		// 폰트 초기화. _pFaceName = family name (등록 폰트 → 시스템 폰트 순 검색).
		bool Initialize(C_DX11_SPRITE_ENGINE* _pEngine,
			const wchar_t* _pFaceName,
			uint32_t _uFontHeight,
			bool _bBold,
			bool _bItalic = false,
			uint32_t _uAtlasSize = DEFAULT_ATLAS_SIZE);

		void Shutdown(C_DX11_SPRITE_ENGINE* _pEngine);

		// 텍스트 출력 — BeginFrame ~ EndFrame 사이.
		// _fX, _fY = 텍스트 좌상단 (ascent 자동 적용).
		void RenderText(C_DX11_SPRITE_ENGINE* _pEngine,
			float _fX, float _fY, const wchar_t* _pText,
			uint32_t _uColor, float _fScale = 1.0f);

		void RenderTextFmt(C_DX11_SPRITE_ENGINE* _pEngine,
			float _fX, float _fY, uint32_t _uColor, float _fScale,
			const wchar_t* _pFmt, ...);

		float MeasureText(C_DX11_SPRITE_ENGINE* _pEngine, const wchar_t* _pText, float _fScale = 1.0f);

		float GetAscent(float _fScale = 1.0f) const { return m_fAscent * _fScale; }
		float GetDescent(float _fScale = 1.0f) const { return m_fDescent * _fScale; }
		float GetLineHeight(float _fScale = 1.0f) const
		{
			return (m_fAscent + m_fDescent + m_fLineGap) * _fScale;
		}

		TextureHandle GetTextureHandle() const { return m_hTexture; }
		uint32_t GetFontHeightPixels() const { return static_cast<uint32_t>(m_fEmSize); }
		uint32_t GetCachedGlyphCount() const { return static_cast<uint32_t>(m_mapGlyphs.size()); }
		bool IsInitialized() const { return m_hTexture != INVALID_TEXTURE && m_pFace != nullptr; }
	};

} // namespace dx11
