#pragma once

#include <memory>
#include "DebugUIWindow.h"

namespace HRZ2
{
	class NxDXGIImpl;
	class NxD3DImpl;
}

namespace HRZ2::DebugUI
{
	bool Initialize(NxDXGIImpl *DXGIImpl);
	void AddWindow(std::shared_ptr<Window> Handle);
	void SetMenuVisible(bool Visible);

	void RenderUI();
	void RenderUID3D(NxD3DImpl *D3DImpl, NxDXGIImpl *DXGIImpl);

	bool ShouldInterceptInput();
	void UpdatePlayerSpecific();
	void UpdateFreecam();
}
