// DxInput.cpp
#include "framework.h"
#include "DxInput.h"


namespace dx11
{

	_DX_INPUT_STATE& Input()
	{
		static _DX_INPUT_STATE s_;
		return s_;
	}

} // namespace dx11
