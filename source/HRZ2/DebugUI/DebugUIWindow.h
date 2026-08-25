#pragma once

namespace HRZ2::DebugUI
{
	class Window
	{
	public:
		virtual ~Window() = default;
		virtual void Render() = 0;
		virtual bool Close() = 0;
		virtual void Reopen() {}
		virtual std::string GetId() const = 0;
	};
}
