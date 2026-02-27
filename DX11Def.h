// DX11Def.h: DX11 시스템 헤더 및 기본 타입 정의
#pragma once

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <cstdint>
#include <vector>


namespace dx11
{

	// 텍스처 핸들 (슬롯 인덱스, -1=무효)
	using TextureHandle = int32_t;
	inline constexpr TextureHandle INVALID_TEXTURE = -1;

	// 블렌드 모드
	enum E_BLEND_MODE : uint8_t
	{
		BLEND_NONE = 0,
		BLEND_ALPHA,
		BLEND_ADDITIVE,
		BLEND_MULTIPLY,
	};

	// 스프라이트 인스턴스 (48바이트, GPU 전송용)
	struct SpriteInstance
	{
		float m_ScreenX, m_ScreenY, m_Width, m_Height;		// 16B: 화면 위치+크기 (픽셀)
		float m_UVLeft, m_UVTop, m_UVRight, m_UVBottom;	// 16B: UV 좌표
		float m_Depth;										// 4B: 깊이값 (0.0~1.0)
		uint32_t m_Color;									// 4B: ARGB 색상
		float m_Pad[2];										// 8B: 16바이트 정렬용
	};
	static_assert(sizeof(SpriteInstance) == 48, "SpriteInstance 크기 48바이트 필수");

	// 텍스처 슬롯
	struct TextureSlot
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pTexture;	// 동적 텍스처용 (UpdateSubresource)
		uint32_t m_uWidth;
		uint32_t m_uHeight;
		bool m_bManaged;		// 엔진 관리 텍스처 (true면 해제 시 Release)

		TextureSlot() : m_uWidth(0), m_uHeight(0), m_bManaged(false) {}
	};

	// 상수 버퍼 구조체 (16바이트 정렬)
	struct CBPerFrame
	{
		float m_fInvScreenWidth;
		float m_fInvScreenHeight;
		float m_fPad[2];
	};
	static_assert(sizeof(CBPerFrame) == 16, "CBPerFrame 16바이트 정렬 필수");

	//========================================================================
	// 3D 메시 타입
	//========================================================================

	// 3D 정점 (위치 + 법선 + UV, 32바이트) — 파일 I/O 및 기본 렌더링용
	struct MeshVertex
	{
		DirectX::XMFLOAT3 m_Position;	// 12B
		DirectX::XMFLOAT3 m_Normal;		// 12B
		DirectX::XMFLOAT2 m_UV;		// 8B
	};
	static_assert(sizeof(MeshVertex) == 32, "MeshVertex 32바이트 필수");

	// 3D 정점 + 탄젠트 (노멀맵 렌더링용, 48바이트)
	struct MeshVertexTBN
	{
		DirectX::XMFLOAT3 m_Position;	// 12B
		DirectX::XMFLOAT3 m_Normal;		// 12B
		DirectX::XMFLOAT2 m_UV;		// 8B
		DirectX::XMFLOAT4 m_Tangent;	// 16B (xyz=탄젠트, w=바이탄젠트 부호)
	};
	static_assert(sizeof(MeshVertexTBN) == 48, "MeshVertexTBN 48바이트 필수");

	// 스킨드 메시 정점 (위치 + 법선 + UV + 본 인덱스 + 본 웨이트, 52바이트)
	struct SkinnedMeshVertex
	{
		DirectX::XMFLOAT3 m_Position;      // 12B
		DirectX::XMFLOAT3 m_Normal;        // 12B
		DirectX::XMFLOAT2 m_UV;            // 8B
		uint8_t           m_BoneIndices[4]; // 4B  (BLENDINDICES, R8G8B8A8_UINT)
		DirectX::XMFLOAT4 m_BoneWeights;   // 16B (BLENDWEIGHT)
	};
	static_assert(sizeof(SkinnedMeshVertex) == 52, "SkinnedMeshVertex 52바이트 필수");

	// 메시 핸들
	using MeshHandle = int32_t;
	inline constexpr MeshHandle INVALID_MESH = -1;

	// 최대 본 수 (상수 버퍼 크기 제약)
	inline constexpr uint32_t MAX_BONES = 54;

	// 3D 프레임 상수 (뷰프로젝션 + 라이팅, 96바이트)
	struct CB3DPerFrame
	{
		DirectX::XMFLOAT4X4 m_ViewProj;	// 64B
		DirectX::XMFLOAT3  m_LightDir;		// 12B
		float              m_Pad0;			// 4B
		DirectX::XMFLOAT3  m_CameraPos;	// 12B
		float              m_Pad1;			// 4B
	};
	static_assert(sizeof(CB3DPerFrame) == 96, "CB3DPerFrame 96바이트 필수");

	// 3D 오브젝트 상수 (월드 행렬, 64바이트)
	struct CB3DPerObject
	{
		DirectX::XMFLOAT4X4 m_World;		// 64B
	};
	static_assert(sizeof(CB3DPerObject) == 64, "CB3DPerObject 64바이트 필수");

	// 스킨드 메시 본 행렬 상수 (b2, 54 * 64B = 3456B)
	struct CB3DBones
	{
		DirectX::XMFLOAT4X4 m_Bones[MAX_BONES];
	};
	static_assert(sizeof(CB3DBones) == MAX_BONES * 64, "CB3DBones 크기 검증");

	// 엔진 통계
	struct EngineStats
	{
		uint32_t m_uFrameCount;
		uint32_t m_uDrawCallsPerFrame;
		uint32_t m_uSpritesPerFrame;
		uint32_t m_uTextureChanges;
		float m_fFPS;

		EngineStats()
			: m_uFrameCount(0)
			, m_uDrawCallsPerFrame(0)
			, m_uSpritesPerFrame(0)
			, m_uTextureChanges(0)
			, m_fFPS(0.0f)
		{
		}
	};

} // namespace dx11
