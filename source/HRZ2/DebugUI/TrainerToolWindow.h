#pragma once

#include <imgui.h>

namespace HRZ2::DebugUI
{
	float GetTrainerUIScale();

	class TrainerToolWindow final
	{
	public:
		TrainerToolWindow(const char *Id, const char *Title, bool *Open, const ImVec2& DefaultSize);
		~TrainerToolWindow();

		TrainerToolWindow(const TrainerToolWindow&) = delete;
		TrainerToolWindow& operator=(const TrainerToolWindow&) = delete;

		explicit operator bool() const { return m_Visible; }

	private:
		bool m_WindowBegun = false;
		bool m_ContentBegun = false;
		bool m_Visible = false;
	};
}
