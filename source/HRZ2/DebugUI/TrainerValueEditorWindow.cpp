#include <algorithm>
#include <cfloat>
#include <format>
#include <imgui.h>
#include "../TrainerCheats.h"
#include "TrainerToolWindow.h"
#include "TrainerValueEditorWindow.h"

namespace
{
	using Feature = HRZ2::TrainerCheats::Feature;
	using Mode = HRZ2::DebugUI::TrainerValueEditorWindow::Mode;

	Feature GetFeature(Mode Value)
	{
		switch (Value)
		{
		case Mode::DamageMultiplier: return Feature::DamageMultiplier;
		case Mode::DefenseMultiplier: return Feature::DefenseMultiplier;
		case Mode::ExperienceMultiplier: return Feature::ExperienceMultiplier;
		case Mode::ToolsAmount: return Feature::EditTools;
		case Mode::AmmoAmount: return Feature::EditAmmo;
		case Mode::ResourcesAmount: return Feature::EditResources;
		case Mode::SkillPoints: return Feature::EditSkillPoints;
		}
		return Feature::EditResources;
	}
}

namespace HRZ2::DebugUI
{
	TrainerValueEditorWindow::TrainerValueEditorWindow(Mode EditorMode)
	{
		s_RequestedMode = EditorMode;
		SelectMode(EditorMode);
	}

	void TrainerValueEditorWindow::SelectMode(Mode EditorMode)
	{
		m_Mode = EditorMode;
		m_Status.clear();
		m_RequestKeyboardFocus = true;

		switch (m_Mode)
		{
		case Mode::DamageMultiplier: m_FloatValue = TrainerCheats::GetDamageMultiplier(); break;
		case Mode::DefenseMultiplier: m_FloatValue = TrainerCheats::GetDefenseMultiplier(); break;
		case Mode::ExperienceMultiplier: m_FloatValue = TrainerCheats::GetExperienceMultiplier(); break;
		case Mode::ToolsAmount:
		case Mode::AmmoAmount:
		case Mode::ResourcesAmount:
			m_IntegerValue = std::max(1U, TrainerCheats::GetItemAmount(GetFeature(m_Mode)));
			break;
		case Mode::SkillPoints:
			m_IntegerValue = std::max(1U, TrainerCheats::GetSkillPoints());
			break;
		}
	}

	const char *TrainerValueEditorWindow::GetTitle() const
	{
		switch (m_Mode)
		{
		case Mode::DamageMultiplier: return "伤害倍率设置";
		case Mode::DefenseMultiplier: return "防御倍率设置";
		case Mode::ExperienceMultiplier: return "经验倍率设置";
		case Mode::ToolsAmount: return "修改工具数量";
		case Mode::AmmoAmount: return "修改弹药数量";
		case Mode::ResourcesAmount: return "修改资源数量";
		case Mode::SkillPoints: return "修改技能点";
		}
		return "数值设置";
	}

	const char *TrainerValueEditorWindow::GetExplanation() const
	{
		if (IsMultiplier())
			return "输入倍率并确认后才会启用。关闭倍率会恢复游戏的正常计算。";
		if (m_Mode == Mode::SkillPoints)
			return "输入目标技能点并确认。修改将在技能点数据下一次更新时执行一次。";
		return "输入目标数量并确认。修改器只处理下一次匹配的物品更新，完成后自动关闭，不会持续锁定数量。";
	}

	bool TrainerValueEditorWindow::IsMultiplier() const
	{
		return m_Mode == Mode::DamageMultiplier || m_Mode == Mode::DefenseMultiplier ||
			m_Mode == Mode::ExperienceMultiplier;
	}

	void TrainerValueEditorWindow::Confirm()
	{
		const auto feature = GetFeature(m_Mode);
		if (!TrainerCheats::IsAvailable(feature))
		{
			m_Status = "当前游戏版本无法使用此修改项。";
			return;
		}

		if (IsMultiplier())
		{
			m_FloatValue = std::clamp(m_FloatValue, 1.0f, 100.0f);
			switch (m_Mode)
			{
			case Mode::DamageMultiplier: TrainerCheats::SetDamageMultiplier(m_FloatValue); break;
			case Mode::DefenseMultiplier: TrainerCheats::SetDefenseMultiplier(m_FloatValue); break;
			case Mode::ExperienceMultiplier: TrainerCheats::SetExperienceMultiplier(m_FloatValue); break;
			default: break;
			}
			TrainerCheats::SetEnabled(feature, true);
			m_Status = std::format("已启用：{:.2f} 倍。", m_FloatValue);
			return;
		}

		m_IntegerValue = std::clamp(m_IntegerValue, 1U, m_Mode == Mode::SkillPoints ? 9999U : 999999U);
		if (m_Mode == Mode::SkillPoints)
		{
			TrainerCheats::SetSkillPoints(m_IntegerValue);
			TrainerCheats::ApplySkillPoints();
			m_Status = std::format("已提交 {} 点；只执行一次。", m_IntegerValue);
		}
		else
		{
			TrainerCheats::ApplyItemAmountOnce(feature, m_IntegerValue);
			m_Status = std::format("已提交数量 {}；下一次匹配后自动关闭。", m_IntegerValue);
		}
	}

	void TrainerValueEditorWindow::DisableOrCancel()
	{
		const auto feature = GetFeature(m_Mode);
		TrainerCheats::SetEnabled(feature, false);
		m_Status = IsMultiplier() ? "倍率已关闭，恢复游戏默认计算。" : "待处理的数量修改已取消。";
	}

	void TrainerValueEditorWindow::Render()
	{
		const float scale = GetTrainerUIScale();
		const auto displaySize = ImGui::GetIO().DisplaySize;
		const float margin = 18.0f * scale;
		const float width = std::min(520.0f * scale, displaySize.x - margin * 2.0f);
		const float height = std::min(390.0f * scale, displaySize.y - margin * 2.0f);
		const float mainMenuX = std::max(margin, std::min(34.0f * scale, displaySize.x - width - margin));
		const float preferredX = mainMenuX + width + 24.0f * scale;
		const float windowX = preferredX + width <= displaySize.x - margin
			? preferredX
			: std::max(margin, displaySize.x - width - margin);
		const float windowY = std::clamp(72.0f * scale, margin, std::max(margin, displaySize.y - height - margin));

		ImGui::SetNextWindowPos(ImVec2(windowX, windowY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f * scale, 18.0f * scale));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, 1.25f * scale));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.030f, 0.038f, 0.995f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.72f, 0.28f, 1.00f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (ImGui::Begin(GetId().c_str(), &m_WindowOpen, flags))
		{
			const auto position = ImGui::GetWindowPos();
			const auto draw = ImGui::GetWindowDrawList();
			const float headerHeight = 64.0f * scale;
			const ImVec2 headerMin = position;
			const ImVec2 headerMax(position.x + width, position.y + headerHeight);
			draw->AddRectFilledMultiColor(headerMin, headerMax,
				IM_COL32(17, 124, 137, 255), IM_COL32(7, 60, 79, 255),
				IM_COL32(4, 39, 53, 255), IM_COL32(10, 83, 96, 255));
			draw->AddRectFilled(ImVec2(headerMin.x, headerMax.y - 4.0f * scale), headerMax, IM_COL32(226, 169, 60, 255));
			draw->AddText(ImVec2(headerMin.x + 20.0f * scale, headerMin.y + 18.0f * scale),
				IM_COL32(255, 255, 255, 255), GetTitle());
			const float closeWidth = headerHeight;
			const ImVec2 closeMin(headerMax.x - closeWidth, headerMin.y);
			ImGui::SetCursorScreenPos(closeMin);
			ImGui::InvisibleButton("##TrainerValueEditorClose", ImVec2(closeWidth, headerHeight - 4.0f * scale));
			if (ImGui::IsItemHovered())
				draw->AddRectFilled(closeMin, ImVec2(headerMax.x, headerMax.y - 4.0f * scale), IM_COL32(188, 58, 50, 235));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				m_WindowOpen = false;
			const auto closeSize = ImGui::CalcTextSize("X");
			draw->AddText(ImVec2(closeMin.x + (closeWidth - closeSize.x) * 0.5f,
				closeMin.y + (headerHeight - closeSize.y) * 0.5f - 2.0f * scale), IM_COL32(255, 255, 255, 255), "X");

			ImGui::SetCursorScreenPos(ImVec2(position.x + 20.0f * scale, headerMax.y + 22.0f * scale));
			ImGui::BeginGroup();
			ImGui::TextWrapped("%s", GetExplanation());
			ImGui::Spacing();
			ImGui::TextUnformatted(IsMultiplier() ? "输入倍率" : "输入数量");
			ImGui::SetNextItemWidth(width - 40.0f * scale);
			if (m_RequestKeyboardFocus)
			{
				ImGui::SetKeyboardFocusHere();
				m_RequestKeyboardFocus = false;
			}
			if (IsMultiplier())
				ImGui::InputFloat("##TrainerFloatValue", &m_FloatValue, 0.0f, 0.0f, "%.2f");
			else
				ImGui::InputScalar("##TrainerIntegerValue", ImGuiDataType_U32, &m_IntegerValue, nullptr, nullptr, "%u");

			ImGui::Spacing();
			const float buttonGap = 12.0f * scale;
			const float buttonWidth = (width - 40.0f * scale - buttonGap) * 0.5f;
			if (ImGui::Button("确认修改", ImVec2(buttonWidth, 50.0f * scale)))
				Confirm();
			ImGui::SameLine(0.0f, buttonGap);
			if (ImGui::Button(IsMultiplier() ? "关闭倍率" : "取消待处理", ImVec2(buttonWidth, 50.0f * scale)))
				DisableOrCancel();

			if (!m_Status.empty())
			{
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "%s", m_Status.c_str());
			}
			ImGui::EndGroup();
		}
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	bool TrainerValueEditorWindow::Close()
	{
		return !m_WindowOpen;
	}

	void TrainerValueEditorWindow::Reopen()
	{
		m_WindowOpen = true;
		SelectMode(s_RequestedMode);
	}

	std::string TrainerValueEditorWindow::GetId() const
	{
		return "HFW Trainer Value Editor";
	}
}
