// DxTypes.h: GUI 위젯 시스템 공용 타입 (point/size/rect/color).
// namespace dx11 — DX11Wrapp 엔진 내장 GUI 라이브러리.
// 외부 의존성 0 (cstdint 만). C_DX_DRAW_CONTEXT / C_DX_WIDGET 등이 사용.
#pragma once

#include <cstdint>


namespace dx11
{

	struct _DX_POINT
	{
		float x;
		float y;

		_DX_POINT() : x(0.0f), y(0.0f) {}
		_DX_POINT(float _x, float _y) : x(_x), y(_y) {}
	};


	struct _DX_SIZE
	{
		float w;
		float h;

		_DX_SIZE() : w(0.0f), h(0.0f) {}
		_DX_SIZE(float _w, float _h) : w(_w), h(_h) {}
	};


	struct _DX_RECT
	{
		float x;
		float y;
		float w;
		float h;

		_DX_RECT() : x(0.0f), y(0.0f), w(0.0f), h(0.0f) {}
		_DX_RECT(float _x, float _y, float _w, float _h)
			: x(_x), y(_y), w(_w), h(_h)
		{
		}

		bool Contains(float _px, float _py) const
		{
			return _px >= x && _px < (x + w)
			    && _py >= y && _py < (y + h);
		}

		_DX_POINT GetCenter() const { return _DX_POINT(x + w * 0.5f, y + h * 0.5f); }
		_DX_POINT GetTopLeft() const { return _DX_POINT(x, y); }
		_DX_SIZE  GetSize() const { return _DX_SIZE(w, h); }
	};


	// ARGB 32비트 — 상위 바이트 alpha. C_DX11_SPRITE_ENGINE::Draw 의 m_uColor 와 동일.
	struct _DX_COLOR
	{
		uint32_t argb;

		_DX_COLOR() : argb(0xFFFFFFFFu) {}
		explicit _DX_COLOR(uint32_t _argb) : argb(_argb) {}

		static _DX_COLOR Rgba(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
		{
			return _DX_COLOR(
				  (static_cast<uint32_t>(_a) << 24)
				| (static_cast<uint32_t>(_r) << 16)
				| (static_cast<uint32_t>(_g) << 8)
				|  static_cast<uint32_t>(_b));
		}

		uint8_t A() const { return static_cast<uint8_t>((argb >> 24) & 0xFFu); }
		uint8_t R() const { return static_cast<uint8_t>((argb >> 16) & 0xFFu); }
		uint8_t G() const { return static_cast<uint8_t>((argb >>  8) & 0xFFu); }
		uint8_t B() const { return static_cast<uint8_t>( argb        & 0xFFu); }
	};

} // namespace dx11
