// DX11MeshRenderer.h: 3D 메시 렌더링 (디렉셔널 라이팅 + 노멀맵)
#pragma once

#include "DX11Def.h"
#include "DX11Device.h"

#include <vector>


namespace dx11
{
	class C_DX11_MESH_RENDERER
	{
	private:
		// 디바이스 참조 (소유하지 않음)
		C_DX11_DEVICE* m_pDevice;

		// 3D 셰이더 (정적 메시, 32바이트 정점)
		Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pAlphaTestPS;  // clip(alpha-0.5)
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pEmissivePS;   // 라이팅 미적용 (가산 블렌딩용)
		Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_pInputLayout;

		// 3D 셰이더 (TBN 정적 메시, 48바이트 정점 — 노멀맵 지원)
		Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pTBNVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pTBNPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pTBNAlphaTestPS;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_pTBNInputLayout;

		// 3D 셰이더 (스킨드 메시)
		Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pSkinnedVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pSkinnedPS;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_pSkinnedInputLayout;

		// 상수 버퍼
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCBPerFrame;		// CB3DPerFrame (b0)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCBPerObject;	// CB3DPerObject (b1)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCBBones;		// CB3DBones (b2)

		// 메시 데이터
		struct MeshData
		{
			Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVB;
			Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIB;
			uint32_t m_uIndexCount;
			uint32_t m_uVertexCount;
			uint32_t m_uVertexStride;  // 32(정적) / 48(TBN) / 52(스킨드)
		};
		std::vector<MeshData> m_vMeshes;

		// 기본 1x1 흰색 텍스처 (텍스처 미지정 시 사용)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pWhiteSRV;

		// 기본 1x1 플랫 노멀맵 (128,128,255 = 탄젠트 스페이스 Z-up)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pFlatNormalSRV;

		// LINEAR + WRAP 샘플러 (UV 타일링 지원 — 존 오브젝트 등)
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pWrapSampler;

		// 가산 블렌딩 (불빛 이펙트 등 ADDITIVE 머티리얼)
		Microsoft::WRL::ComPtr<ID3D11BlendState>        m_pAdditiveBlendState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthReadOnlyState;

		bool m_bInitialized;
		bool m_bAlphaTestActive;  // EnableAlphaTest 상태 추적 (TBN PS 선택용)
		bool m_bAdditiveActive;   // EnableAdditiveBlend 상태 추적 (TBN 스킵용)

		bool compileShaders();
		bool compileTBNShaders();
		bool compileSkinnedShaders();
		bool createConstantBuffers();
		bool createDefaultTexture();

	public:
		C_DX11_MESH_RENDERER();
		~C_DX11_MESH_RENDERER();

		bool Initialize(C_DX11_DEVICE* _pDevice);
		void Shutdown();

		// 메시 생성 (정적, 32바이트)
		MeshHandle CreateMesh(const MeshVertex* _pVerts, uint32_t _uVertCount,
			const uint32_t* _pIndices, uint32_t _uIndexCount);

		// 메시 생성 (정적 + 탄젠트, 48바이트 — 노멀맵 렌더링용)
		MeshHandle CreateMeshTBN(const MeshVertexTBN* _pVerts, uint32_t _uVertCount,
			const uint32_t* _pIndices, uint32_t _uIndexCount);

		// 메시 생성 (스킨드)
		MeshHandle CreateSkinnedMesh(const SkinnedMeshVertex* _pVerts, uint32_t _uVertCount,
			const uint32_t* _pIndices, uint32_t _uIndexCount);

		// 프레임 렌더링
		void BeginFrame(const DirectX::XMFLOAT4X4& _rViewProj,
			const DirectX::XMFLOAT3& _rLightDir,
			const DirectX::XMFLOAT3& _rCameraPos);
		void DrawMesh(MeshHandle _hMesh, const DirectX::XMFLOAT4X4& _rWorld,
			ID3D11ShaderResourceView* _pDiffuseSRV = nullptr,
			ID3D11ShaderResourceView* _pNormalSRV = nullptr);
		void DrawSkinnedMesh(MeshHandle _hMesh, const DirectX::XMFLOAT4X4& _rWorld,
			const DirectX::XMFLOAT4X4* _pBones, uint32_t _uBoneCount,
			ID3D11ShaderResourceView* _pSRV = nullptr);
		void EndFrame();

		// 알파 테스트 PS 전환 (SpeedTree 잎/관목 전용)
		void EnableAlphaTest(bool _bEnable);

		// 가산 블렌딩 전환 (불빛 이펙트 등 ADDITIVE 머티리얼)
		void EnableAdditiveBlend(bool _bEnable);

		// 양면 렌더링 전환 (TWOSIDED 머티리얼)
		void EnableTwoSided(bool _bEnable);

		bool IsInitialized() const { return m_bInitialized; }
	};

} // namespace dx11
