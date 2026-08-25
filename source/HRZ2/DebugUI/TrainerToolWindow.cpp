#include <algorithm>
#include <cfloat>
#include "TrainerToolWindow.h"

namespace HRZ2::DebugUI
{
	TrainerToolWindow::TrainerToolWindow(const char *Id, const char *Title, bool *Open, const ImVec2& DefaultSize)
	{
		ImGui::SetNextWindowSize(DefaultSize, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.030f, 0.038f, 0.985f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.89f, 0.66f, 0.24f, 0.95f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		m_WindowBegun = true;
		m_Visible = ImGui::Begin(Id, Open, flags);
		if (!m_Visible)
			return;

		const auto windowPosition = ImGui::GetWindowPos();
		const auto windowSize = ImGui::GetWindowSize();
		const float headerHeight = std::max(50.0f, ImGui::GetFontSize() * 2.55f);
		const float closeWidth = headerHeight;
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
			ImVec2(headerMin.x, headerMax.y - 3.0f),
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
		ImGui::InvisibleButton("##TrainerToolClose", ImVec2(closeWidth, headerHeight - 3.0f));
		const bool closeHovered = ImGui::IsItemHovered();
		if (closeHovered)
			draw->AddRectFilled(closeMin, ImVec2(headerMax.x, headerMax.y - 3.0f), IM_COL32(168, 54, 48, 210));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && Open)
		{
			*Open = false;
			m_Visible = false;
		}

		const auto titleSize = ImGui::CalcTextSize(Title);
		draw->AddText(
			ImVec2(headerMin.x + 16.0f, headerMin.y + (headerHeight - titleSize.y) * 0.5f - 1.0f),
			IM_COL32(242, 249, 249, 255),
			Title);
		const char *closeLabel = "X";
		const auto closeLabelSize = ImGui::CalcTextSize(closeLabel);
		draw->AddText(
			ImVec2(closeMin.x + (closeWidth - closeLabelSize.x) * 0.5f, closeMin.y + (headerHeight - closeLabelSize.y) * 0.5f - 1.0f),
			closeHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(188, 221, 223, 255),
			closeLabel);

		if (!m_Visible)
			return;

		ImGui::SetCursorScreenPos(ImVec2(windowPosition.x, windowPosition.y + headerHeight));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
		ImGui::BeginChild("##TrainerToolContent", ImVec2(0.0f, 0.0f), false);
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
