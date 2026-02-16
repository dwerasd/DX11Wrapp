// DX11SpriteEngine.h: 2D 스프라이트 렌더링 엔진 (싱글톤, 인스턴스드 배칭)
// DX9Wrapp C_DX9_SPRITE_ENGINE 패턴 계승
#pragma once

#include "DX11Def.h"
#include "DX11Device.h"
#include "DX11Texture.h"

#include <vector>


namespace dx11
{

	class C_DX11_SPRITE_ENGINE
	{
	private:
		// 싱글톤
		static C_DX11_SPRITE_ENGINE* s_pInstance;

		static constexpr UINT MAX_INSTANCES = 16384;
		static constexpr UINT MAX_TEXTURES = 4096;

		// 디바이스
		C_DX11_DEVICE m_Device;

		// 셰이더
		Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pPS;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_pInputLayout;

		// 버퍼
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;		// 유닛 쿼드 (4정점)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIndexBuffer;		// 쿼드 인덱스 (6)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pInstanceBuffer;		// 인스턴스 데이터 (동적)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBuffer;		// 프레임 상수

		// 인스턴스 CPU 스테이징
		SpriteInstance* m_pInstances;
		UINT m_uInstanceCount;

		// 텍스처 슬롯
		std::vector<TextureSlot> m_vTextures;
		TextureHandle m_hCurrentTexture;

		// 상태
		bool m_bInitialized;
		EngineStats m_Stats;

		// 내부 함수
		bool compileShaders();
		bool createBuffers();
		void flush();

		// 생성/소멸 (private - 싱글톤)
		C_DX11_SPRITE_ENGINE();
		~C_DX11_SPRITE_ENGINE();
		C_DX11_SPRITE_ENGINE(const C_DX11_SPRITE_ENGINE&) = delete;
		C_DX11_SPRITE_ENGINE& operator=(const C_DX11_SPRITE_ENGINE&) = delete;

	public:
		//========================================================================
		// 싱글톤 접근
		//========================================================================
		static C_DX11_SPRITE_ENGINE* GetInstance();
		static void DestroyInstance();

		//========================================================================
		// 초기화/해제
		//========================================================================
		bool Initialize(HWND _hWnd, UINT _uWidth, UINT _uHeight, bool _bWindowed);
		void Shutdown();

		//========================================================================
		// 텍스처 관리
		//========================================================================
		TextureHandle LoadTexture(const wchar_t* _pPath);
		TextureHandle CreateTexture(const void* _pData, UINT _uW, UINT _uH);
		void ReleaseTexture(TextureHandle _hTex);

		//========================================================================
		// 렌더링
		//========================================================================
		void BeginFrame(float _fR, float _fG, float _fB, float _fA);
		void Draw(TextureHandle _hTex,
			float _fX, float _fY, float _fW, float _fH,
			float _fU0, float _fV0, float _fU1, float _fV1,
			float _fDepth, uint32_t _uColor);
		void EndFrame();
		void Present(UINT _uSync);

		//========================================================================
		// 유틸리티
		//========================================================================
		C_DX11_DEVICE* GetDevice() { return &m_Device; }
		const EngineStats& GetStats() const { return m_Stats; }
		UINT GetScreenWidth()  const { return m_Device.GetWidth(); }
		UINT GetScreenHeight() const { return m_Device.GetHeight(); }
		bool IsInitialized()   const { return m_bInitialized; }

		// 스크린샷 (백버퍼 → BMP 파일 저장)
		bool SaveScreenshot(const wchar_t* _pPath);

		// 동적 텍스처 (UpdateSubresource 지원)
		TextureHandle CreateDynamicTexture(UINT _uW, UINT _uH);
		void UpdateTextureRegion(TextureHandle _hTex,
			UINT _uX, UINT _uY, UINT _uW, UINT _uH, const void* _pData);

		// 샘플러 전환 (폰트: linear, 픽셀아트: point)
		void SetLinearFiltering(bool _bLinear);
	};

} // namespace dx11
