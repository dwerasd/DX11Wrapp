// DX12Font.h: 동적 글리프 아틀라스 폰트 시스템 (RFont 패턴).
// JotaX C_GAME_UI 폰트 시스템을 라이브러리화 — GDI on-demand 글리프 렌더링 →
// DX12 동적 텍스처 셀 업로드 → 스프라이트 엔진 quad tint 출력.
// 외부 의존성 0 (FreeType/stb_truetype 사용 안 함, OS GDI 만 사용).
#pragma once

#include "DX11Def.h"

#include <unordered_map>


namespace dx11
{
	class C_DX11_SPRITE_ENGINE;


	// 동적 글리프 정보 (아틀라스 셀 인덱스 + 너비/높이).
	struct _DX11_FONT_GLYPH
	{
		uint16_t m_uCellIndex;	// 아틀라스 셀 인덱스 (0 ~ FONT_CELL_COUNT-1)
		uint8_t m_uWidth;		// 글리프 advance 너비 (픽셀)
		uint8_t m_uHeight;		// 글리프 높이 (픽셀)
	};


	class C_DX11_FONT
	{
	private:
		// 기본 아틀라스 / 셀 크기 (Initialize 인자로 override 가능).
		static constexpr uint32_t DEFAULT_ATLAS_SIZE = 1024;
		static constexpr uint32_t DEFAULT_CELL_SIZE = 32;

		// 스프라이트 엔진의 텍스처 핸들 (CreateDynamicTexture 결과).
		TextureHandle m_hTexture;

		// 아틀라스 메타.
		uint32_t m_uAtlasSize;		// 텍스처 한 변 픽셀 (예: 1024)
		uint32_t m_uCellSize;		// 셀 한 변 픽셀 (예: 32)
		uint32_t m_uCellsX;			// m_uAtlasSize / m_uCellSize
		uint32_t m_uCellsY;
		uint32_t m_uCellCount;		// m_uCellsX * m_uCellsY

		// 폰트 설정 (스케일 1.0 기준 글리프 높이).
		uint32_t m_uFontHeight;

		// GDI 글리프 렌더링 (on-demand).
		HDC m_hDC;
		HFONT m_hFont;
		HBITMAP m_hBitmap;
		uint32_t* m_pBits;			// DIBSection 픽셀 (m_uCellSize × m_uCellSize)

		// 동적 글리프 캐시.
		std::unordered_map<wchar_t, _DX11_FONT_GLYPH> m_mapGlyphs;
		uint32_t m_uNextCell;

		// 캐시 미스 시 GDI 로 글리프 렌더 + 텍스처 업로드.
		const _DX11_FONT_GLYPH* getGlyph_(C_DX11_SPRITE_ENGINE* _pEngine, wchar_t _wChar);

	public:
		C_DX11_FONT();
		~C_DX11_FONT();
		C_DX11_FONT(const C_DX11_FONT&) = delete;
		C_DX11_FONT& operator=(const C_DX11_FONT&) = delete;

		// 폰트 시스템 초기화.
		// _pFaceName    : 폰트 페이스 (예: L"맑은 고딕", L"Pretendard Variable").
		// _uFontHeight  : 픽셀 단위 글리프 높이 (CreateFontW 의 음수 lfHeight).
		// _bBold        : 굵게 여부.
		// _uCellSize    : 셀 한 변 (default 32). _uFontHeight 보다 충분히 커야 함.
		// _uAtlasSize   : 아틀라스 한 변 (default 1024). 셀 개수 = (atlas/cell)^2.
		// 주의 — BeginFrame 외부에서 호출. 내부 CreateDynamicTexture 가 일회용 cmdlist 로
		//        텍스처를 0 초기화 + COPY_DEST→PIXEL_SHADER_RESOURCE 전환하기 때문.
		bool Initialize(C_DX11_SPRITE_ENGINE* _pEngine,
			const wchar_t* _pFaceName,
			uint32_t _uFontHeight,
			bool _bBold,
			uint32_t _uCellSize = DEFAULT_CELL_SIZE,
			uint32_t _uAtlasSize = DEFAULT_ATLAS_SIZE);

		void Shutdown(C_DX11_SPRITE_ENGINE* _pEngine);

		// 텍스트 출력 — BeginFrame ~ EndFrame 사이 호출.
		// _fScale: 1.0 = m_uFontHeight 픽셀 그대로, 0.8 = 축소, 2.0 = 확대 (point sampling 으로 끊김 가능).
		// _uColor: ARGB 32비트 (스프라이트 엔진 tint, 알파 마스크 글리프에 곱해짐).
		// Windows 의 DrawText 매크로와 충돌 회피 위해 RenderText 명명.
		void RenderText(C_DX11_SPRITE_ENGINE* _pEngine,
			float _fX, float _fY, const wchar_t* _pText,
			uint32_t _uColor, float _fScale);

		// printf 스타일 포맷 출력 (내부 버퍼 512 wchar_t).
		void RenderTextFmt(C_DX11_SPRITE_ENGINE* _pEngine,
			float _fX, float _fY, uint32_t _uColor, float _fScale,
			const wchar_t* _pFmt, ...);

		// 텍스트 픽셀 너비 측정 (렌더링 없이 캐시 채움 — hit-test/정렬용).
		// 캐시 미스 글리프도 atlas 에 등록되므로 직후 RenderText 호출은 동일 글자에 대해 캐시 hit.
		float MeasureText(C_DX11_SPRITE_ENGINE* _pEngine, const wchar_t* _pText, float _fScale);

		// 접근자.
		TextureHandle GetTextureHandle() const { return m_hTexture; }
		uint32_t GetFontHeight() const { return m_uFontHeight; }
		uint32_t GetCellSize() const { return m_uCellSize; }
		uint32_t GetCachedGlyphCount() const { return m_uNextCell; }
		bool IsInitialized() const { return m_hTexture != INVALID_TEXTURE; }
	};

} // namespace dx11
