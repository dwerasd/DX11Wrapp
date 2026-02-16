// DX11MeshRenderer.cpp: 3D 메시 렌더링 구현
#include "framework.h"
#include "DX11MeshRenderer.h"

#include <cstring>


//============================================================================
// 3D 셰이더 소스
//============================================================================
static const char* const g_pMeshVS = R"HLSL(
cbuffer CBPerFrame : register(b0)
{
	float4x4 g_ViewProj;
	float3   g_LightDir;
	float    g_Pad0;
	float3   g_CameraPos;
	float    g_Pad1;
};

cbuffer CBPerObject : register(b1)
{
	float4x4 g_World;
};

struct VS_INPUT
{
	float3 Pos    : POSITION;
	float3 Normal : NORMAL;
	float2 UV     : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Pos      : SV_POSITION;
	float3 WorldPos : TEXCOORD0;
	float3 Normal   : TEXCOORD1;
	float2 UV       : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	float4 worldPos = mul(float4(input.Pos, 1.0), g_World);
	output.Pos = mul(worldPos, g_ViewProj);
	output.WorldPos = worldPos.xyz;
	output.Normal = normalize(mul(input.Normal, (float3x3)g_World));
	output.UV = input.UV;
	return output;
}
)HLSL";

static const char* const g_pMeshPS = R"HLSL(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

cbuffer CBPerFrame : register(b0)
{
	float4x4 g_ViewProj;
	float3   g_LightDir;
	float    g_Pad0;
	float3   g_CameraPos;
	float    g_Pad1;
};

struct PS_INPUT
{
	float4 Pos      : SV_POSITION;
	float3 WorldPos : TEXCOORD0;
	float3 Normal   : TEXCOORD1;
	float2 UV       : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 N = normalize(input.Normal);
	float3 L = normalize(-g_LightDir);
	float NdotL = saturate(dot(N, L));

	float3 ambient = float3(0.15, 0.15, 0.2);
	float3 diffuse = float3(1.0, 1.0, 1.0) * NdotL;

	float4 texColor = g_Texture.Sample(g_Sampler, input.UV);
	float3 finalColor = texColor.rgb * (ambient + diffuse);

	return float4(finalColor, texColor.a);
}
)HLSL";


namespace dx11
{

	C_DX11_MESH_RENDERER::C_DX11_MESH_RENDERER()
		: m_pDevice(nullptr)
		, m_bInitialized(false)
	{
	}

	C_DX11_MESH_RENDERER::~C_DX11_MESH_RENDERER()
	{
		Shutdown();
	}

	bool C_DX11_MESH_RENDERER::Initialize(C_DX11_DEVICE* _pDevice)
	{
		if (!_pDevice || !_pDevice->IsInitialized())
			return false;

		m_pDevice = _pDevice;

		if (!compileShaders())
			return false;

		if (!createConstantBuffers())
			return false;

		if (!createDefaultTexture())
			return false;

		m_bInitialized = true;
		DBGPRINT(L"[DX11Mesh] 메시 렌더러 초기화 완료");
		return true;
	}

	void C_DX11_MESH_RENDERER::Shutdown()
	{
		if (!m_bInitialized) return;

		m_vMeshes.clear();
		m_pWhiteSRV.Reset();
		m_pCBPerObject.Reset();
		m_pCBPerFrame.Reset();
		m_pInputLayout.Reset();
		m_pPS.Reset();
		m_pVS.Reset();

		m_pDevice = nullptr;
		m_bInitialized = false;
		DBGPRINT(L"[DX11Mesh] 메시 렌더러 종료");
	}

	//============================================================================
	// 셰이더 컴파일
	//============================================================================
	bool C_DX11_MESH_RENDERER::compileShaders()
	{
		ID3D11Device* const pDev = m_pDevice->GetDevice();

		// VS
		Microsoft::WRL::ComPtr<ID3DBlob> pVSBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

		HRESULT hr = ::D3DCompile(
			g_pMeshVS, std::strlen(g_pMeshVS),
			"MeshVS", nullptr, nullptr,
			"main", "vs_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&pVSBlob, &pErrorBlob
		);
		if (FAILED(hr))
		{
			if (pErrorBlob)
				DBGPRINT(L"[DX11Mesh] VS 컴파일 에러: %S", static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			return false;
		}

		hr = pDev->CreateVertexShader(
			pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
			nullptr, &m_pVS);
		if (FAILED(hr)) return false;

		// PS
		Microsoft::WRL::ComPtr<ID3DBlob> pPSBlob;
		hr = ::D3DCompile(
			g_pMeshPS, std::strlen(g_pMeshPS),
			"MeshPS", nullptr, nullptr,
			"main", "ps_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&pPSBlob, &pErrorBlob
		);
		if (FAILED(hr))
		{
			if (pErrorBlob)
				DBGPRINT(L"[DX11Mesh] PS 컴파일 에러: %S", static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			return false;
		}

		hr = pDev->CreatePixelShader(
			pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
			nullptr, &m_pPS);
		if (FAILED(hr)) return false;

		// 인풋 레이아웃 (MeshVertex: Position + Normal + UV)
		const D3D11_INPUT_ELEMENT_DESC aLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = pDev->CreateInputLayout(
			aLayout, _countof(aLayout),
			pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
			&m_pInputLayout);
		if (FAILED(hr))
		{
			DBGPRINT(L"[DX11Mesh] InputLayout 생성 실패: 0x%08X", hr);
			return false;
		}

		DBGPRINT(L"[DX11Mesh] 3D 셰이더 컴파일 완료");
		return true;
	}

	//============================================================================
	// 상수 버퍼 생성
	//============================================================================
	bool C_DX11_MESH_RENDERER::createConstantBuffers()
	{
		ID3D11Device* const pDev = m_pDevice->GetDevice();

		D3D11_BUFFER_DESC cbDesc{};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		cbDesc.ByteWidth = sizeof(CB3DPerFrame);
		HRESULT hr = pDev->CreateBuffer(&cbDesc, nullptr, &m_pCBPerFrame);
		if (FAILED(hr)) return false;

		cbDesc.ByteWidth = sizeof(CB3DPerObject);
		hr = pDev->CreateBuffer(&cbDesc, nullptr, &m_pCBPerObject);
		if (FAILED(hr)) return false;

		return true;
	}

	//============================================================================
	// 기본 1x1 흰색 텍스처 (텍스처 미지정 시 폴백)
	//============================================================================
	bool C_DX11_MESH_RENDERER::createDefaultTexture()
	{
		ID3D11Device* const pDev = m_pDevice->GetDevice();

		constexpr uint32_t uWhite = 0xFFFFFFFF;
		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = 1;
		texDesc.Height = 1;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = &uWhite;
		initData.SysMemPitch = 4;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTex;
		HRESULT hr = pDev->CreateTexture2D(&texDesc, &initData, &pTex);
		if (FAILED(hr)) return false;

		hr = pDev->CreateShaderResourceView(pTex.Get(), nullptr, &m_pWhiteSRV);
		return SUCCEEDED(hr);
	}

	//============================================================================
	// 메시 생성
	//============================================================================
	MeshHandle C_DX11_MESH_RENDERER::CreateMesh(
		const MeshVertex* _pVerts, uint32_t _uVertCount,
		const uint32_t* _pIndices, uint32_t _uIndexCount)
	{
		if (!m_pDevice || !_pVerts || !_pIndices || (_uVertCount == 0) || (_uIndexCount == 0))
			return INVALID_MESH;

		ID3D11Device* const pDev = m_pDevice->GetDevice();
		MeshData mesh_{};
		mesh_.m_uVertexCount = _uVertCount;
		mesh_.m_uIndexCount = _uIndexCount;

		D3D11_BUFFER_DESC vbDesc{};
		vbDesc.ByteWidth = sizeof(MeshVertex) * _uVertCount;
		vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vbData{};
		vbData.pSysMem = _pVerts;

		HRESULT hr = pDev->CreateBuffer(&vbDesc, &vbData, &mesh_.m_pVB);
		if (FAILED(hr)) return INVALID_MESH;

		D3D11_BUFFER_DESC ibDesc{};
		ibDesc.ByteWidth = sizeof(uint32_t) * _uIndexCount;
		ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA ibData{};
		ibData.pSysMem = _pIndices;

		hr = pDev->CreateBuffer(&ibDesc, &ibData, &mesh_.m_pIB);
		if (FAILED(hr)) return INVALID_MESH;

		const MeshHandle hMesh_ = static_cast<MeshHandle>(m_vMeshes.size());
		m_vMeshes.push_back(std::move(mesh_));
		return hMesh_;
	}

	//============================================================================
	// 3D 프레임 시작
	//============================================================================
	void C_DX11_MESH_RENDERER::BeginFrame(
		const DirectX::XMFLOAT4X4& _rViewProj,
		const DirectX::XMFLOAT3& _rLightDir,
		const DirectX::XMFLOAT3& _rCameraPos)
	{
		ID3D11DeviceContext* const pCtx = m_pDevice->GetContext();

		// 3D 스테이트
		m_pDevice->SetDepthEnabled(true);
		m_pDevice->SetCullBack(true);
		m_pDevice->SetLinearSampler(true);

		// Per-Frame 상수 버퍼
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = pCtx->Map(m_pCBPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			CB3DPerFrame* const pCB = static_cast<CB3DPerFrame*>(mapped.pData);
			pCB->m_ViewProj = _rViewProj;
			pCB->m_LightDir = _rLightDir;
			pCB->m_Pad0 = 0.0f;
			pCB->m_CameraPos = _rCameraPos;
			pCB->m_Pad1 = 0.0f;
			pCtx->Unmap(m_pCBPerFrame.Get(), 0);
		}

		// 파이프라인
		pCtx->IASetInputLayout(m_pInputLayout.Get());
		pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCtx->VSSetShader(m_pVS.Get(), nullptr, 0);
		pCtx->PSSetShader(m_pPS.Get(), nullptr, 0);
		pCtx->VSSetConstantBuffers(0, 1, m_pCBPerFrame.GetAddressOf());
		pCtx->VSSetConstantBuffers(1, 1, m_pCBPerObject.GetAddressOf());
		pCtx->PSSetConstantBuffers(0, 1, m_pCBPerFrame.GetAddressOf());
	}

	//============================================================================
	// 메시 드로우
	//============================================================================
	void C_DX11_MESH_RENDERER::DrawMesh(
		MeshHandle _hMesh,
		const DirectX::XMFLOAT4X4& _rWorld,
		ID3D11ShaderResourceView* _pSRV)
	{
		if ((_hMesh < 0) || (_hMesh >= static_cast<MeshHandle>(m_vMeshes.size())))
			return;

		const MeshData& mesh_ = m_vMeshes[_hMesh];
		ID3D11DeviceContext* const pCtx = m_pDevice->GetContext();

		// Per-Object 상수 버퍼
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = pCtx->Map(m_pCBPerObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			CB3DPerObject* const pCB = static_cast<CB3DPerObject*>(mapped.pData);
			pCB->m_World = _rWorld;
			pCtx->Unmap(m_pCBPerObject.Get(), 0);
		}

		// 텍스처 (없으면 흰색 폴백)
		ID3D11ShaderResourceView* const pSRV_ = _pSRV ? _pSRV : m_pWhiteSRV.Get();
		pCtx->PSSetShaderResources(0, 1, &pSRV_);

		// VB/IB
		constexpr UINT uStride_ = sizeof(MeshVertex);
		constexpr UINT uOffset_ = 0;
		ID3D11Buffer* const pVB_ = mesh_.m_pVB.Get();
		pCtx->IASetVertexBuffers(0, 1, &pVB_, &uStride_, &uOffset_);
		pCtx->IASetIndexBuffer(mesh_.m_pIB.Get(), DXGI_FORMAT_R32_UINT, 0);

		pCtx->DrawIndexed(mesh_.m_uIndexCount, 0, 0);
	}

	//============================================================================
	// 3D 프레임 종료
	//============================================================================
	void C_DX11_MESH_RENDERER::EndFrame()
	{
		ID3D11DeviceContext* const pCtx = m_pDevice->GetContext();
		ID3D11ShaderResourceView* const pNull_ = nullptr;
		pCtx->PSSetShaderResources(0, 1, &pNull_);
	}

} // namespace dx11
