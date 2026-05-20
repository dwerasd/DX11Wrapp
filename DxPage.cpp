// DxPage.cpp
#include "framework.h"
#include "DxPage.h"
#include "DxLabel.h"
#include "DxButton.h"
#include "DxEditbox.h"
#include "DxCheckbox.h"
#include "DxDrawContext.h"

#include <nlohmann/json.hpp>
#include <fstream>


namespace dx11
{

	C_DX_WIDGET* C_DX_PAGE::Add(std::unique_ptr<C_DX_WIDGET> _p)
	{
		if (!_p) { return nullptr; }
		m_vWidgets.push_back(std::move(_p));
		return m_vWidgets.back().get();
	}


	bool C_DX_PAGE::Remove(const std::string& _sKey)
	{
		for (auto it_ = m_vWidgets.begin(); it_ != m_vWidgets.end(); ++it_)
		{
			if ((*it_)->GetKey() == _sKey)
			{
				m_vWidgets.erase(it_);
				return true;
			}
		}
		return false;
	}


	C_DX_WIDGET* C_DX_PAGE::Find(const std::string& _sKey)
	{
		for (auto& p_ : m_vWidgets)
		{
			if (p_ && p_->GetKey() == _sKey) { return p_.get(); }
		}
		return nullptr;
	}


	void C_DX_PAGE::Render(C_DX_DRAW_CONTEXT& _ctx, _DX_POINT _origin)
	{
		for (auto& p_ : m_vWidgets)
		{
			if (p_ && p_->IsVisible()) { p_->Render(_ctx, _origin); }
		}
	}


	std::unique_ptr<C_DX_WIDGET> DxCreateWidget(const std::string& _sType)
	{
		if (_sType == "label")    { return std::unique_ptr<C_DX_WIDGET>(new C_DX_LABEL()); }
		if (_sType == "button")   { return std::unique_ptr<C_DX_WIDGET>(new C_DX_BUTTON()); }
		if (_sType == "editbox")  { return std::unique_ptr<C_DX_WIDGET>(new C_DX_EDITBOX()); }
		if (_sType == "checkbox") { return std::unique_ptr<C_DX_WIDGET>(new C_DX_CHECKBOX()); }
		return nullptr;
	}


	// ─── 직렬화 헬퍼 (Page 내부 전용) ───────────────────────────────
	// 타입별 추가 필드는 정적 다운캐스트로 처리(GetType 으로 분기).
	// 데이터 포인터(편집박스/체크박스)·콜백(버튼)은 영속 안 함 — Load 후 재바인딩.

	static nlohmann::json SerializeWidget_(const C_DX_WIDGET* _w)
	{
		nlohmann::json j_;
		j_["type"]    = _w->GetTypeName();
		j_["key"]     = _w->GetKey();
		j_["name"]    = _w->GetName();
		const _DX_RECT& r_ = _w->GetRect();
		j_["x"] = r_.x;  j_["y"] = r_.y;  j_["w"] = r_.w;  j_["h"] = r_.h;
		j_["fs"]      = _w->GetFontScale();
		j_["visible"] = _w->IsVisible();
		j_["enabled"] = _w->IsEnabled();
		switch (_w->GetType())
		{
		case DX_WIDGET_LABEL:
		{
			const C_DX_LABEL* p_ = static_cast<const C_DX_LABEL*>(_w);
			j_["color"] = p_->GetColor().argb;
			j_["align"] = static_cast<int>(p_->GetAlign());
			break;
		}
		case DX_WIDGET_BUTTON:
		{
			const C_DX_BUTTON* p_ = static_cast<const C_DX_BUTTON*>(_w);
			j_["style"] = static_cast<int>(p_->GetStyle());
			break;
		}
		case DX_WIDGET_EDITBOX:
		{
			const C_DX_EDITBOX* p_ = static_cast<const C_DX_EDITBOX*>(_w);
			j_["data_type"] = static_cast<int>(p_->GetDataType());
			break;
		}
		default:
			break;
		}
		return j_;
	}


	static void DeserializeWidget_(C_DX_WIDGET* _w, const nlohmann::json& _j)
	{
		_w->SetKey(_j.value("key", std::string()));
		_w->SetName(_j.value("name", std::string()));
		_DX_RECT r_(_j.value("x", 0.0f), _j.value("y", 0.0f),
		            _j.value("w", 0.0f), _j.value("h", 0.0f));
		_w->SetRect(r_);
		_w->SetFontScale(_j.value("fs", 1.0f));
		_w->SetVisible(_j.value("visible", true));
		_w->SetEnabled(_j.value("enabled", true));
		switch (_w->GetType())
		{
		case DX_WIDGET_LABEL:
		{
			C_DX_LABEL* p_ = static_cast<C_DX_LABEL*>(_w);
			if (_j.contains("color"))
			{
				p_->SetColor(_DX_COLOR(_j["color"].get<uint32_t>()));
			}
			if (_j.contains("align"))
			{
				p_->SetAlign(static_cast<E_DX_TEXT_ALIGN>(_j["align"].get<int>()));
			}
			break;
		}
		case DX_WIDGET_BUTTON:
		{
			C_DX_BUTTON* p_ = static_cast<C_DX_BUTTON*>(_w);
			if (_j.contains("style"))
			{
				p_->SetStyle(static_cast<E_DX_BUTTON_STYLE>(_j["style"].get<int>()));
			}
			break;
		}
		default:
			break;
		}
	}


	bool C_DX_PAGE::SaveToFile(const std::wstring& _wsPath) const
	{
		try
		{
			nlohmann::json arr_ = nlohmann::json::array();
			for (const auto& p_ : m_vWidgets)
			{
				if (p_) { arr_.push_back(SerializeWidget_(p_.get())); }
			}
			nlohmann::json doc_;
			doc_["widgets"] = arr_;

			std::ofstream ofs_(_wsPath, std::ios::trunc);
			if (!ofs_.is_open()) { return false; }
			ofs_ << doc_.dump(2);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}


	bool C_DX_PAGE::LoadFromFile(const std::wstring& _wsPath)
	{
		try
		{
			std::ifstream ifs_(_wsPath);
			if (!ifs_.is_open()) { return false; }
			nlohmann::json doc_;
			ifs_ >> doc_;
			if (!doc_.contains("widgets") || !doc_["widgets"].is_array())
			{
				return false;
			}
			m_vWidgets.clear();
			for (const auto& wj_ : doc_["widgets"])
			{
				if (!wj_.is_object()) { continue; }
				const std::string sType_ = wj_.value("type", std::string());
				auto p_ = DxCreateWidget(sType_);
				if (!p_) { continue; }
				DeserializeWidget_(p_.get(), wj_);
				m_vWidgets.push_back(std::move(p_));
			}
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

} // namespace dx11
