#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "Core/WorldPosition.h"

namespace HRZ2
{
	class Player;

	namespace TrainerCheats
	{
		enum class Feature : uint8_t
		{
			IgnoreHits,
			InfiniteHealth,
			InfiniteFocus,
			InfiniteValor,
			InfiniteSkillDuration,
			InfiniteOxygen,
			MaxMedicinePouch,
			InfiniteArrowsAndTraps,
			InstantBowCharge,
			SuperDamage,
			DamageMultiplier,
			DefenseMultiplier,
			EditTools,
			EditAmmo,
			EditResources,
			IgnoreCraftingRequirements,
			StealthMode,
			FreezeTrialTimer,
			ExperienceMultiplier,
			GrantExperience,
			EditSkillPoints,
			TeleportToWaypoint,
			Count,
		};

		void Initialize();
		void UpdatePlayerState(Player *PlayerInstance);

		bool IsAvailable(Feature Value);
		bool IsEnabled(Feature Value);
		std::string_view GetUnavailableReason(Feature Value);
		void SetEnabled(Feature Value, bool Enabled);

		float GetDamageMultiplier();
		void SetDamageMultiplier(float Value);
		float GetDefenseMultiplier();
		void SetDefenseMultiplier(float Value);
		float GetExperienceMultiplier();
		void SetExperienceMultiplier(float Value);

		uint32_t GetItemAmount(Feature Value);
		void SetItemAmount(Feature Value, uint32_t Amount);
		uint32_t GetSkillPoints();
		void SetSkillPoints(uint32_t Amount);
		void ApplySkillPoints();
		void GrantExperience();

		std::optional<WorldPosition> GetWaypointPosition();
	}
}
