// DxPage.h: 위젯 컨테이너 + JSON 영속.
// vector<unique_ptr<C_DX_WIDGET>> 소유. 추가/제거/조회/렌더 API +
// SaveToFile / LoadFromFile (nlohmann/json).
// Bindings(데이터 포인터)·onClick 콜백은 Load 후 코드가 재등록해야 함
// (포인터·함수 직렬화는 불가).
#pragma once

#include "DxWidget.h"

#include <memory>
#include <string>
#include <vector>


namespace dx11
{
	class C_DX_DRAW_CONTEXT;


	class C_DX_PAGE
	{
	private:
		std::vector<std::unique_ptr<C_DX_WIDGET>> m_vWidgets;

	public:
		C_DX_PAGE() = default;
		~C_DX_PAGE() = default;

		C_DX_PAGE(const C_DX_PAGE&) = delete;
		C_DX_PAGE& operator=(const C_DX_PAGE&) = delete;

		// 추가 — 소유권 이전. 반환=내부 raw 포인터(추가 설정용).
		C_DX_WIDGET* Add(std::unique_ptr<C_DX_WIDGET> _p);

		// 키로 제거. 못 찾으면 false.
		bool Remove(const std::string& _sKey);

		// 키로 검색. 없으면 nullptr.
		C_DX_WIDGET* Find(const std::string& _sKey);

		// 인덱스 접근(편집창 순회용).
		size_t       Count() const { return m_vWidgets.size(); }
		C_DX_WIDGET* At(size_t _i)
		{ return _i < m_vWidgets.size() ? m_vWidgets[_i].get() : nullptr; }

		void Clear() { m_vWidgets.clear(); }

		// 렌더 — _origin = 페이지 절대 screen pos (모든 위젯의 x/y 가 더해짐).
		void Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin);

		// JSON 영속.
		bool SaveToFile(const std::wstring& _wsPath) const;
		bool LoadFromFile(const std::wstring& _wsPath);
	};


	// 타입 이름("label"/"button"/"editbox"/"checkbox") → 위젯 생성 팩토리.
	std::unique_ptr<C_DX_WIDGET> DxCreateWidget(const std::string& _sType);

} // namespace dx11
