// DX11MeshRenderer.h: 3D 메시 렌더링 (기본 디렉셔널 라이팅)
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

		// 3D 셰이더 (정적 메시)
		Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pPS;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_pInputLayout;

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
			uint32_t m_uVertexStride;  // 32(정적) 또는 52(스킨드)
		};
		std::vector<MeshData> m_vMeshes;

		// 기본 1x1 흰색 텍스처 (텍스처 미지정 시 사용)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pWhiteSRV;

		bool m_bInitialized;

		bool compileShaders();
		bool compileSkinnedShaders();
		bool createConstantBuffers();
		bool createDefaultTexture();

	public:
		C_DX11_MESH_RENDERER();
		~C_DX11_MESH_RENDERER();

		bool Initialize(C_DX11_DEVICE* _pDevice);
		void Shutdown();

		// 메시 생성 (정적)
		MeshHandle CreateMesh(const MeshVertex* _pVerts, uint32_t _uVertCount,
			const uint32_t* _pIndices, uint32_t _uIndexCount);

		// 메시 생성 (스킨드)
		MeshHandle CreateSkinnedMesh(const SkinnedMeshVertex* _pVerts, uint32_t _uVertCount,
			const uint32_t* _pIndices, uint32_t _uIndexCount);

		// 프레임 렌더링
		void BeginFrame(const DirectX::XMFLOAT4X4& _rViewProj,
			const DirectX::XMFLOAT3& _rLightDir,
			const DirectX::XMFLOAT3& _rCameraPos);
		void DrawMesh(MeshHandle _hMesh, const DirectX::XMFLOAT4X4& _rWorld,
			ID3D11ShaderResourceView* _pSRV = nullptr);
		void DrawSkinnedMesh(MeshHandle _hMesh, const DirectX::XMFLOAT4X4& _rWorld,
			const DirectX::XMFLOAT4X4* _pBones, uint32_t _uBoneCount,
			ID3D11ShaderResourceView* _pSRV = nullptr);
		void EndFrame();

		bool IsInitialized() const { return m_bInitialized; }
	};

} // namespace dx11
