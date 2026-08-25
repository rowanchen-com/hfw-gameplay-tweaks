#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "../Core/WorldTransform.h"
#include "DebugUIWindow.h"

namespace HRZ2::DebugUI
{
	class MainMenuBar : public Window
	{
	public:
		enum class FreeCamMode
		{
			Off,
			Free,
			Noclip,
		};

		static inline bool m_IsVisible;

		static inline FreeCamMode m_FreeCamMode;
		static inline WorldTransform m_FreeCamPosition;

		static inline bool m_PauseGame;
		static inline bool m_PauseAIProcessing;
		static inline bool m_TimescaleOverride;
		static inline bool m_TimescaleOverrideInMenus;
		static inline float m_Timescale = 1.0f;
		static inline float m_LODRangeModifier = 1.0f;

		static inline bool m_EnableGodMode;
		static inline bool m_EnableDemigodMode;
		static inline bool m_EnableInfiniteClipAmmo;
		static inline bool m_EnableAutoNeutralFaction;

		MainMenuBar();
		virtual void Render() override;
		virtual bool Close() override;
		virtual std::string GetId() const override;

		static void ToggleVisibility();
		static void TogglePauseGameLogic();
		static void TogglePauseAIProcessing();
		static void TogglePauseTimeOfDay();
		static void ToggleQuickSave();
		static void ToggleQuickLoad();
		static void ToggleTimescaleOverride();
		static void AdjustTimescale(float Adjustment);
		static void ToggleFreeflyCamera();
		static void ToggleNoclip();

	private:
		enum class Page : uint8_t
		{
			Home,
			Player,
			Combat,
			Survival,
			Resources,
			World,
			Teleport,
			Faction,
			Utilities,
			Developer,
			ConfirmExit,
		};

		struct NavigationState
		{
			Page PageId = Page::Home;
			size_t SelectedIndex = 0;
			size_t ScrollOffset = 0;
		};

		struct MenuItem
		{
			std::string Label;
			std::string Value;
			std::string Description;
			bool Enabled = true;
			bool IsSubmenu = false;
			std::function<void()> Activate;
			std::function<void()> AdjustLeft;
			std::function<void()> AdjustRight;
		};

		std::vector<NavigationState> m_Navigation { NavigationState {} };
		static inline std::optional<WorldPosition> m_SavedPosition;
		static inline std::optional<WorldPosition> m_UndoPosition;

		std::vector<MenuItem> BuildMenuItems();
		void DrawTrainerFrame(std::vector<MenuItem>& Items);
		bool HandleMenuInput(std::vector<MenuItem>& Items);
		void ActivateItem(MenuItem& Item);
		void OpenPage(Page Target);
		void GoBack();
		const char *GetPageTitle() const;

		static void TeleportTo(const WorldPosition& Position);
		static void DumpPlayerComponents();
	};
}
