#include <algorithm>
#include "TrainerToolWindow.h"

namespace HRZ2::DebugUI
{
	float GetTrainerUIScale()
	{
		const float displayHeight = ImGui::GetIO().DisplaySize.y;
		return std::clamp(displayHeight / 1080.0f, 1.00f, 2.00f);
	}

	TrainerToolWindow::TrainerToolWindow(const char *Id, const char *Title, bool *Open, const ImVec2& DefaultSize)
	{
		const float scale = GetTrainerUIScale();
		const auto displaySize = ImGui::GetIO().DisplaySize;
		const float screenMargin = 16.0f * scale;
		const float mainMenuWidth = std::min(600.0f * scale, displaySize.x - 36.0f * scale);
		const float mainMenuX = std::max(
			18.0f * scale,
			std::min(34.0f * scale, displaySize.x - mainMenuWidth - 18.0f * scale));
		const float preferredX = mainMenuX + mainMenuWidth + 24.0f * scale;
		const float availableRightWidth = displaySize.x - preferredX - screenMargin;
		const ImVec2 screenMaximumSize(
			std::max(360.0f, displaySize.x - 32.0f * scale),
			std::max(320.0f, displaySize.y - 32.0f * scale));
		const float minimumWidth = std::min(900.0f * scale, screenMaximumSize.x);
		const bool canPlaceBesideMainMenu = availableRightWidth >= minimumWidth;
		const ImVec2 maximumSize(
			canPlaceBesideMainMenu ? availableRightWidth : screenMaximumSize.x,
			screenMaximumSize.y);
		const ImVec2 minimumSize(
			std::min(minimumWidth, maximumSize.x),
			std::min(950.0f * scale, maximumSize.y));
		const ImVec2 scaledDefaultSize(
			std::clamp(DefaultSize.x * scale, minimumSize.x, maximumSize.x),
			std::clamp(DefaultSize.y * scale, minimumSize.y, maximumSize.y));
		const float toolWindowX = canPlaceBesideMainMenu
			? preferredX
			: std::max(screenMargin, displaySize.x - scaledDefaultSize.x - screenMargin);
		const float maximumY = std::max(screenMargin, displaySize.y - scaledDefaultSize.y - screenMargin);
		const float toolWindowY = std::clamp(72.0f * scale, screenMargin, maximumY);

		ImGui::SetNextWindowPos(ImVec2(toolWindowX, toolWindowY), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(scaledDefaultSize, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(minimumSize, maximumSize);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, 1.25f * scale));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.030f, 0.038f, 0.995f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.72f, 0.28f, 1.00f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		m_WindowBegun = true;
		m_Visible = ImGui::Begin(Id, Open, flags);
		if (!m_Visible)
			return;

		const auto windowPosition = ImGui::GetWindowPos();
		const auto windowSize = ImGui::GetWindowSize();
		const float headerHeight = std::max(58.0f * scale, ImGui::GetFontSize() * 2.15f);
		const float closeWidth = headerHeight;
		const float dividerHeight = std::max(3.0f, 3.0f * scale);
		auto draw = ImGui::GetWindowDrawList();

		const ImVec2 headerMin = windowPosition;
		const ImVec2 headerMax(windowPosition.x + windowSize.x, windowPosition.y + headerHeight);
		draw->AddRectFilledMultiColor(
			headerMin,
			headerMax,
			IM_COL32(17, 124, 137, 255),
			IM_COL32(7, 60, 79, 255),
			IM_COL32(4, 39, 53, 255),
			IM_COL32(10, 83, 96, 255));
		draw->AddRectFilled(
			ImVec2(headerMin.x, headerMax.y - dividerHeight),
			headerMax,
			IM_COL32(226, 169, 60, 255));

		ImGui::SetCursorScreenPos(headerMin);
		ImGui::InvisibleButton("##TrainerToolDrag", ImVec2(std::max(1.0f, windowSize.x - closeWidth), headerHeight));
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			const auto delta = ImGui::GetIO().MouseDelta;
			ImGui::SetWindowPos(ImVec2(windowPosition.x + delta.x, windowPosition.y + delta.y));
		}

		const ImVec2 closeMin(headerMax.x - closeWidth, headerMin.y);
		ImGui::SetCursorScreenPos(closeMin);
		ImGui::InvisibleButton("##TrainerToolClose", ImVec2(closeWidth, headerHeight - dividerHeight));
		const bool closeHovered = ImGui::IsItemHovered();
		if (closeHovered)
			draw->AddRectFilled(closeMin, ImVec2(headerMax.x, headerMax.y - dividerHeight), IM_COL32(188, 58, 50, 235));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && Open)
		{
			*Open = false;
			m_Visible = false;
		}

		const auto titleSize = ImGui::CalcTextSize(Title);
		draw->AddText(
			ImVec2(headerMin.x + 18.0f * scale, headerMin.y + (headerHeight - titleSize.y) * 0.5f - 1.0f),
			IM_COL32(255, 255, 255, 255),
			Title);
		const char *closeLabel = "X";
		const auto closeLabelSize = ImGui::CalcTextSize(closeLabel);
		draw->AddText(
			ImVec2(closeMin.x + (closeWidth - closeLabelSize.x) * 0.5f, closeMin.y + (headerHeight - closeLabelSize.y) * 0.5f - 1.0f),
			IM_COL32(255, 255, 255, 255),
			closeLabel);

		if (!m_Visible)
			return;

		ImGui::SetCursorScreenPos(ImVec2(windowPosition.x, windowPosition.y + headerHeight));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * scale, 16.0f * scale));
		ImGui::BeginChild(
			"##TrainerToolContent",
			ImVec2(0.0f, 0.0f),
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::SetScrollX(0.0f);
		ImGui::SetScrollY(0.0f);
		ImGui::PopStyleVar();
		m_ContentBegun = true;
	}

	TrainerToolWindow::~TrainerToolWindow()
	{
		if (m_ContentBegun)
			ImGui::EndChild();
		if (m_WindowBegun)
			ImGui::End();

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}
}
