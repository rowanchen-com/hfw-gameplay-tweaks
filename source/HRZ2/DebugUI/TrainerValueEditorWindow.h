#pragma once

#include <cstdint>
#include <string>
#include "DebugUIWindow.h"

namespace HRZ2::DebugUI
{
	class TrainerValueEditorWindow final : public Window
	{
	public:
		enum class Mode : uint8_t
		{
			DamageMultiplier,
			DefenseMultiplier,
			ExperienceMultiplier,
			InfiniteJump,
			MovementSpeed,
			FallSpeed,
			ToolsAmount,
			AmmoAmount,
			ResourcesAmount,
			SkillPoints,
		};

		explicit TrainerValueEditorWindow(Mode EditorMode);

		virtual void Render() override;
		virtual bool Close() override;
		virtual void Reopen() override;
		virtual std::string GetId() const override;

	private:
		static inline Mode s_RequestedMode = Mode::ToolsAmount;

		bool m_WindowOpen = true;
		bool m_RequestKeyboardFocus = true;
		Mode m_Mode = Mode::ToolsAmount;
		uint32_t m_IntegerValue = 1;
		float m_FloatValue = 2.0f;
		std::string m_Status;

		void SelectMode(Mode EditorMode);
		const char *GetTitle() const;
		const char *GetExplanation() const;
		bool IsMultiplier() const;
		bool UsesFloatValue() const;
		void Confirm();
		void DisableOrCancel();
	};
}
