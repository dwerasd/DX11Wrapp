# DX11Wrapp

DirectX 11 그래픽스 고성능 추상화 레이어 래퍼 라이브러리.

> **참고용 프로젝트** — 이 프로젝트는 단독으로 빌드할 수 없습니다. 코드 참고 목적으로만 공개합니다.

## 개요

2D 스프라이트 배칭과 3D 메시 렌더링을 위한 DirectX 11 래퍼로, 스키닝 애니메이션, 노멀 매핑, 방향성 라이팅 등을 지원한다.

## 주요 기능

### 디바이스 관리
- **C_DX11_DEVICE** - D3D11 디바이스, 스왑 체인, 렌더 상태 관리
- Feature Level 11.0, DXGI 1.2 지원
- 블렌드/샘플러/래스터라이저/깊이-스텐실 상태 관리
- 윈도우 리사이즈 및 풀스크린 지원

### 2D 스프라이트 엔진
- **C_DX11_SPRITE_ENGINE** - 인스턴싱 기반 스프라이트 배칭 렌더러 (싱글톤)
- 프레임당 최대 16,384개 스프라이트 배치 처리
- 텍스처 슬롯 최대 4,096개
- 텍스처 변경 시 자동 플러시
- 동적 텍스처 업데이트 (애니메이션)
- 스크린샷 저장 (BMP)

### 3D 메시 렌더러
- **C_DX11_MESH_RENDERER** - 라이팅/노멀맵/스키닝 지원 3D 렌더러
- 3가지 정점 포맷:
  - Static (32B) - Position + Normal + UV
  - TBN (48B) - + Tangent (노멀 매핑용)
  - Skinned (52B) - + BoneIndices + BoneWeights (최대 54본)
- 방향성 라이트 + 반구형 앰비언트 라이팅
- 거리/높이 기반 포그
- ACES 톤 매핑
- 알파 테스트, Additive 블렌딩, 양면 렌더링

### 셰이더 시스템
- HLSL 소스를 C++ 문자열로 임베딩, 런타임 컴파일 (D3DCompile)
- 2D 스프라이트 VS/PS (vs_5_0, ps_5_0)
- 3D 메시 VS/PS (Static, TBN, Skinned 각각)

### 텍스처 시스템
- WIC 기반 텍스처 로딩 (PNG, JPG, BMP, TIFF 등)
- RGBA 32비트 자동 변환
- IMMUTABLE / DYNAMIC 용도 분리

### 구성
| 구성 | 설명 |
|------|------|
| Debug | 디버그 빌드 (C++20) |
| Release | 릴리즈 빌드 (C++Latest, 정적 런타임) |
| ReleaseMD | 릴리즈 빌드 (C++Latest, 동적 런타임) |

Win32 및 x64 플랫폼 지원. 출력물은 정적 라이브러리(.lib).

### 링크 라이브러리
```
d3d11.lib  dxgi.lib  d3dcompiler.lib  windowscodecs.lib
```

## 구조

```
DX11Wrapp/
├── DX11Device             디바이스 및 렌더 상태 관리
├── DX11SpriteEngine       2D 인스턴싱 스프라이트 배칭 엔진
├── DX11MeshRenderer       3D 메시 렌더러 (라이팅/노멀맵/스키닝)
├── DX11Texture            WIC 기반 텍스처 로딩
└── DX11Def                타입 정의, 구조체, 열거형
```

## 네임스페이스

모든 타입과 함수는 `dx11` 네임스페이스에 정의된다.

## 라이선스
이 프로젝트는 MIT License에 따라 배포됩니다.

---

*이 프로젝트는 Claude Opus 4.6을 활용하여 작성되었습니다.*