#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <Windows.h>
#include <xbyak/xbyak.h>
#include "Core/Destructibility.h"
#include "Core/Entity.h"
#include "Core/Player.h"
#include "TrainerCheats.h"

namespace
{
	using HRZ2::TrainerCheats::Feature;
	using namespace Xbyak::util;

	constexpr size_t FeatureCount = static_cast<size_t>(Feature::Count);
	constexpr float InfiniteHealthValue = 9999.0f;
	constexpr float SuperDamageValue = 999999.0f;
	constexpr float MinimumValorValue = 100.0f;
	constexpr float FullBowChargeValue = 100.0f;
	constexpr float OneValue = 1.0f;

	std::array<std::atomic_uint32_t, FeatureCount> g_enabled {};
	std::array<std::atomic_bool, FeatureCount> g_available {};
	std::array<std::string, FeatureCount> g_unavailableReasons {};
	std::atomic_bool g_initialized = false;

	std::atomic<float> g_damageMultiplier = 2.0f;
	std::atomic<float> g_defenseMultiplier = 2.0f;
	std::atomic<float> g_experienceMultiplier = 2.0f;
	std::atomic_uint32_t g_toolsAmount = 999;
	std::atomic_uint32_t g_ammoAmount = 999;
	std::atomic_uint32_t g_resourcesAmount = 999;
	std::atomic_uint32_t g_skillPoints = 99;
	std::atomic_uint32_t g_applySkillPoints = 0;
	std::atomic_uint32_t g_grantExperience = 0;

	std::atomic_uintptr_t g_playerEntity = 0;
	std::atomic_uintptr_t g_playerHealth = 0;
	std::atomic_uint32_t g_itemType = 0;
	std::atomic_uint32_t g_itemMaximumStack = 0;
	std::atomic_uint32_t g_shouldEditItem = 0;
	std::atomic_uintptr_t g_itemPointerCheck = 0;
	std::atomic<double> g_waypointX = 0.0;
	std::atomic<double> g_waypointY = 0.0;
	std::atomic<double> g_waypointZ = 0.0;

	std::vector<std::unique_ptr<Xbyak::CodeGenerator>> g_hookCode;
	std::uintptr_t g_moduleBase = 0;
	std::size_t g_moduleSize = 0;
	std::uintptr_t g_codeBegin = 0;
	std::uintptr_t g_codeEnd = 0;
	std::uint8_t *g_relayPage = nullptr;
	std::size_t g_relayOffset = 0;

	constexpr size_t ToIndex(Feature Value)
	{
		return static_cast<size_t>(Value);
	}

	std::uintptr_t FlagAddress(Feature Value)
	{
		return reinterpret_cast<std::uintptr_t>(&g_enabled[ToIndex(Value)]);
	}

	void SetAvailability(Feature Value, bool Available, std::string Reason = {})
	{
		if (!Available && Reason.empty())
			Reason = "当前游戏版本未找到对应特征码。";
		g_available[ToIndex(Value)].store(Available, std::memory_order_release);
		g_unavailableReasons[ToIndex(Value)] = Available ? std::string {} : std::move(Reason);
	}

	struct TogglePatch
	{
		std::uintptr_t Address = 0;
		std::vector<std::uint8_t> Original;
		std::vector<std::uint8_t> Enabled;
		bool Active = false;

		bool Configure(std::uintptr_t PatchAddress, std::initializer_list<std::uint8_t> OriginalBytes,
			std::initializer_list<std::uint8_t> EnabledBytes)
		{
			if (!PatchAddress || OriginalBytes.size() != EnabledBytes.size())
				return false;

			Address = PatchAddress;
			Original.assign(OriginalBytes);
			Enabled.assign(EnabledBytes);
			return true;
		}

		void Set(bool Value)
		{
			if (!Address || Active == Value)
				return;

			const auto& bytes = Value ? Enabled : Original;
			Memory::Patch(Address, bytes.data(), bytes.size());
			Active = Value;
		}
	};

	TogglePatch g_oxygenPatch;
	TogglePatch g_skillDurationPatch;
	TogglePatch g_craftingPatch;
	TogglePatch g_disableAIPatch;

	std::vector<int16_t> ParsePattern(std::string_view Text)
	{
		std::vector<int16_t> result;
		for (size_t i = 0; i < Text.size();)
		{
			while (i < Text.size() && Text[i] == ' ')
				++i;
			if (i >= Text.size())
				break;

			const size_t start = i;
			while (i < Text.size() && Text[i] != ' ')
				++i;
			const auto token = Text.substr(start, i - start);
			if (token.empty())
				continue;

			if (token.front() == '?' || token.front() == '*')
			{
				result.emplace_back(-1);
				continue;
			}

			unsigned value = 0;
			const auto conversion = std::from_chars(token.data(), token.data() + token.size(), value, 16);
			if (conversion.ec != std::errc {} || conversion.ptr != token.data() + token.size() || value > 0xFF)
				return {};
			result.emplace_back(static_cast<int16_t>(value));
		}
		return result;
	}

	std::uintptr_t FindPattern(std::string_view Pattern, std::uintptr_t Begin = 0, std::uintptr_t End = 0)
	{
		const auto bytes = ParsePattern(Pattern);
		if (bytes.empty())
			return 0;

		if (!Begin)
			Begin = g_codeBegin;
		if (!End)
			End = g_codeEnd;
		if (Begin >= End || static_cast<size_t>(End - Begin) < bytes.size())
			return 0;

		size_t anchor = 0;
		while (anchor < bytes.size() && bytes[anchor] < 0)
			++anchor;
		if (anchor == bytes.size())
			return Begin;

		const auto first = reinterpret_cast<const std::uint8_t *>(Begin);
		const auto last = reinterpret_cast<const std::uint8_t *>(End - bytes.size() + anchor + 1);
		const auto anchorByte = static_cast<std::uint8_t>(bytes[anchor]);
		for (auto cursor = first + anchor; cursor < last;)
		{
			cursor = std::find(cursor, last, anchorByte);
			if (cursor == last)
				break;

			const auto candidate = cursor - anchor;
			bool matches = true;
			for (size_t i = 0; i < bytes.size(); ++i)
			{
				if (bytes[i] >= 0 && candidate[i] != static_cast<std::uint8_t>(bytes[i]))
				{
					matches = false;
					break;
				}
			}
			if (matches)
				return reinterpret_cast<std::uintptr_t>(candidate);
			++cursor;
		}
		return 0;
	}

	bool InitializeModuleRange()
	{
		const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		if (!module)
			return false;

		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return false;
		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(module + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return false;

		g_moduleBase = module;
		g_moduleSize = nt->OptionalHeader.SizeOfImage;
		const auto firstSection = IMAGE_FIRST_SECTION(nt);
		for (uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
		{
			const auto& section = firstSection[i];
			if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
				continue;
			const auto begin = module + section.VirtualAddress;
			const auto end = begin + std::max(section.Misc.VirtualSize, section.SizeOfRawData);
			if (!g_codeBegin || begin < g_codeBegin)
				g_codeBegin = begin;
			g_codeEnd = std::max(g_codeEnd, end);
		}
		return g_moduleSize != 0 && g_codeBegin != 0 && g_codeEnd > g_codeBegin;
	}

	std::uint8_t *AllocateRelayPage()
	{
		if (g_relayPage)
			return g_relayPage;

		SYSTEM_INFO systemInfo {};
		GetSystemInfo(&systemInfo);
		const auto granularity = static_cast<std::uintptr_t>(systemInfo.dwAllocationGranularity);
		const auto alignUp = [granularity](std::uintptr_t Value)
		{
			return (Value + granularity - 1) & ~(granularity - 1);
		};

		const auto scanBegin = alignUp(g_moduleBase + g_moduleSize);
		const auto scanEnd = std::min<std::uintptr_t>(scanBegin + 0x60000000ULL,
			std::numeric_limits<std::uintptr_t>::max() - granularity);
		for (auto cursor = scanBegin; cursor < scanEnd;)
		{
			MEMORY_BASIC_INFORMATION information {};
			if (!VirtualQuery(reinterpret_cast<void *>(cursor), &information, sizeof(information)))
				break;

			const auto regionStart = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
			const auto regionEnd = regionStart + information.RegionSize;
			if (information.State == MEM_FREE)
			{
				const auto candidate = alignUp(std::max(cursor, regionStart));
				if (candidate + 0x1000 <= regionEnd)
				{
					g_relayPage = static_cast<std::uint8_t *>(VirtualAlloc(
						reinterpret_cast<void *>(candidate), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
					if (g_relayPage)
						return g_relayPage;
				}
			}
			cursor = std::max(cursor + granularity, regionEnd);
		}
		return nullptr;
	}

	template<typename Builder>
	bool InstallMidHook(std::string_view Name, std::uintptr_t Target, size_t OverwriteLength, Builder&& Build)
	{
		if (!Target || OverwriteLength < 5 || !AllocateRelayPage() || g_relayOffset + 16 > 0x1000)
		{
			spdlog::warn("Trainer feature '{}' is unavailable: hook location or relay allocation failed.", Name);
			return false;
		}

		auto generator = std::make_unique<Xbyak::CodeGenerator>(4096);
		Build(*generator);
		generator->jmp(ptr[rip]);
		generator->dq(Target + OverwriteLength);
		generator->ready();

		const auto code = reinterpret_cast<std::uintptr_t>(generator->getCode<void *>());
		auto relay = g_relayPage + g_relayOffset;
		g_relayOffset += 16;
		std::array<std::uint8_t, 14> relayBytes { 0xFF, 0x25, 0, 0, 0, 0 };
		std::memcpy(relayBytes.data() + 6, &code, sizeof(code));
		Memory::Patch(reinterpret_cast<std::uintptr_t>(relay), relayBytes.data(), relayBytes.size());

		const auto relative64 = reinterpret_cast<std::intptr_t>(relay) - static_cast<std::intptr_t>(Target + 5);
		if (relative64 < std::numeric_limits<int32_t>::min() || relative64 > std::numeric_limits<int32_t>::max())
		{
			spdlog::warn("Trainer feature '{}' is unavailable: relay is outside rel32 range.", Name);
			return false;
		}

		std::vector<std::uint8_t> patch(OverwriteLength, 0x90);
		patch[0] = 0xE9;
		const auto relative = static_cast<int32_t>(relative64);
		std::memcpy(patch.data() + 1, &relative, sizeof(relative));
		Memory::Patch(Target, patch.data(), patch.size());
		g_hookCode.emplace_back(std::move(generator));
		spdlog::info("Trainer feature hook '{}' installed at 0x{:X}.", Name, Target);
		return true;
	}

	void EmitFlagCheck(Xbyak::CodeGenerator& Code, Feature Value, Xbyak::Label& Disabled)
	{
		Code.push(r11);
		Code.mov(r11, FlagAddress(Value));
		Code.cmp(dword[r11], 0);
		Code.pop(r11);
		Code.je(Disabled);
	}

	void EmitOriginal(Xbyak::CodeGenerator& Code, std::initializer_list<std::uint8_t> Bytes)
	{
		for (const auto value : Bytes)
			Code.db(value);
	}

	template<typename T>
	T ReadValue(std::uintptr_t Address)
	{
		T result {};
		std::memcpy(&result, reinterpret_cast<const void *>(Address), sizeof(result));
		return result;
	}

	void ConfigureSimplePatches();
	void InstallCodeHooks();
}

namespace HRZ2::TrainerCheats
{
	void Initialize()
	{
		if (g_initialized.exchange(true))
			return;

		for (size_t i = 0; i < FeatureCount; ++i)
		{
			g_enabled[i].store(0);
			g_available[i].store(false);
			g_unavailableReasons[i] = "当前游戏版本未找到对应特征码。";
		}

		// This one uses the engine's already mapped Destructibility structure and does not require an AOB hook.
		SetAvailability(Feature::InfiniteHealth, true);

		if (!InitializeModuleRange())
		{
			spdlog::warn("Trainer features were not initialized because the game module range is unavailable.");
			return;
		}

		ConfigureSimplePatches();
		InstallCodeHooks();

		size_t availableCount = 0;
		for (const auto& value : g_available)
			availableCount += value.load() ? 1 : 0;
		spdlog::info("Trainer feature initialization complete: {}/{} integrations available.", availableCount, FeatureCount);
	}

	void UpdatePlayerState(Player *PlayerInstance)
	{
		auto entity = PlayerInstance ? PlayerInstance->m_Entity : nullptr;
		g_playerEntity.store(reinterpret_cast<std::uintptr_t>(entity), std::memory_order_release);
		auto health = entity ? entity->m_Destructibility : nullptr;
		g_playerHealth.store(reinterpret_cast<std::uintptr_t>(health), std::memory_order_release);

		if (health && IsEnabled(Feature::InfiniteHealth))
			health->m_Health = InfiniteHealthValue;
	}

	bool IsAvailable(Feature Value)
	{
		return g_available[ToIndex(Value)].load(std::memory_order_acquire);
	}

	bool IsEnabled(Feature Value)
	{
		return g_enabled[ToIndex(Value)].load(std::memory_order_acquire) != 0;
	}

	std::string_view GetUnavailableReason(Feature Value)
	{
		return g_unavailableReasons[ToIndex(Value)];
	}

	void SetEnabled(Feature Value, bool Enabled)
	{
		if (!IsAvailable(Value))
			Enabled = false;

		switch (Value)
		{
		case Feature::InfiniteOxygen: g_oxygenPatch.Set(Enabled); break;
		case Feature::InfiniteSkillDuration: g_skillDurationPatch.Set(Enabled); break;
		case Feature::IgnoreCraftingRequirements: g_craftingPatch.Set(Enabled); break;
		case Feature::StealthMode: g_disableAIPatch.Set(Enabled); break;
		default: break;
		}
		g_enabled[ToIndex(Value)].store(Enabled ? 1U : 0U, std::memory_order_release);
	}

	float GetDamageMultiplier() { return g_damageMultiplier.load(); }
	void SetDamageMultiplier(float Value) { g_damageMultiplier.store(std::clamp(Value, 1.0f, 100.0f)); }
	float GetDefenseMultiplier() { return g_defenseMultiplier.load(); }
	void SetDefenseMultiplier(float Value) { g_defenseMultiplier.store(std::clamp(Value, 1.0f, 100.0f)); }
	float GetExperienceMultiplier() { return g_experienceMultiplier.load(); }
	void SetExperienceMultiplier(float Value) { g_experienceMultiplier.store(std::clamp(Value, 1.0f, 100.0f)); }

	uint32_t GetItemAmount(Feature Value)
	{
		switch (Value)
		{
		case Feature::EditTools: return g_toolsAmount.load();
		case Feature::EditAmmo: return g_ammoAmount.load();
		case Feature::EditResources: return g_resourcesAmount.load();
		default: return 0;
		}
	}

	void SetItemAmount(Feature Value, uint32_t Amount)
	{
		Amount = std::clamp(Amount, 1U, 9999U);
		switch (Value)
		{
		case Feature::EditTools: g_toolsAmount.store(Amount); break;
		case Feature::EditAmmo: g_ammoAmount.store(Amount); break;
		case Feature::EditResources: g_resourcesAmount.store(Amount); break;
		default: break;
		}
	}

	uint32_t GetSkillPoints() { return g_skillPoints.load(); }
	void SetSkillPoints(uint32_t Amount) { g_skillPoints.store(std::clamp(Amount, 1U, 9999U)); }
	void ApplySkillPoints() { if (IsAvailable(Feature::EditSkillPoints)) g_applySkillPoints.store(1); }
	void GrantExperience() { if (IsAvailable(Feature::GrantExperience)) g_grantExperience.store(1); }

	std::optional<WorldPosition> GetWaypointPosition()
	{
		if (!IsAvailable(Feature::TeleportToWaypoint))
			return std::nullopt;
		const WorldPosition result(g_waypointX.load(), g_waypointY.load(), g_waypointZ.load() + 10.0);
		if (result.X == 0.0 && result.Y == 0.0 && result.Z == 10.0)
			return std::nullopt;
		return result;
	}
}

namespace
{
	void ConfigureSimplePatches()
	{
		const auto oxygen = FindPattern("C5 FA 58 4B ? C5 F2 5F CE C5 EA 5D C1 C5 FA 11 ? ? 8B BB ? ? 00 00 85");
		const bool oxygenAvailable = oxygen && g_oxygenPatch.Configure(
			oxygen + 5, { 0xC5, 0xF2, 0x5F, 0xCE }, { 0xF3, 0x0F, 0x10, 0xD1 });
		SetAvailability(Feature::InfiniteOxygen, oxygenAvailable);

		const auto skillDuration = FindPattern("C5 FA 10 4A 1C ? 8D ? ? ? 00 00 FF");
		const bool skillDurationAvailable = skillDuration && g_skillDurationPatch.Configure(
			skillDuration, { 0xC5, 0xFA, 0x10, 0x4A, 0x1C }, { 0x0F, 0x57, 0xC9, 0x66, 0x90 });
		SetAvailability(Feature::InfiniteSkillDuration, skillDurationAvailable);

		const auto crafting = FindPattern("44 3B 77 20 ? 8B ? 24");
		const bool craftingAvailable = crafting && g_craftingPatch.Configure(
			crafting, { 0x44, 0x3B, 0x77, 0x20 }, { 0xB0, 0x63, 0x84, 0xC0 });
		SetAvailability(Feature::IgnoreCraftingRequirements, craftingAvailable);

		const auto disableAI = FindPattern("74 ? ? 38 ? ? ? 00 00 74 ? ? 88 ? ? ? 00 00 ? 8B ? FF ? ? ? 00 00 ? 8B ? ? ? 00 00");
		const bool disableAIAvailable = disableAI && g_disableAIPatch.Configure(disableAI, { 0x74 }, { 0xEB });
		if (!disableAIAvailable)
			SetAvailability(Feature::StealthMode, false);
	}

	void InstallCodeHooks()
	{
		const auto medicine = FindPattern("4C 8B F0 48 85 C0 0F 84 ? ? 00 00 83 7B 64 00 0F 8E ? ? 00 00");
		const bool medicineAvailable = InstallMidHook("MaxMedicinePouch", medicine, 6, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original;
			EmitFlagCheck(code, Feature::MaxMedicinePouch, original);
			code.push(rcx);
			code.mov(ecx, dword[rbx + 0x68]);
			code.mov(dword[rbx + 0x64], ecx);
			code.pop(rcx);
			code.L(original);
			EmitOriginal(code, { 0x4C, 0x8B, 0xF0, 0x48, 0x85, 0xC0 });
		});
		SetAvailability(Feature::MaxMedicinePouch, medicineAvailable);

		const auto focusBase = FindPattern("80 78 ? 00 74 ? C5 FA 10 40 3C C5 F8 2F C1 0F 87 ? ? 00 00");
		const bool focusAvailable = InstallMidHook("InfiniteFocus", focusBase ? focusBase + 6 : 0, 5, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original, complete;
			EmitFlagCheck(code, Feature::InfiniteFocus, original);
			code.movss(xmm0, dword[rax + 0x38]);
			code.movss(dword[rax + 0x3C], xmm0);
			code.jmp(complete);
			code.L(original);
			EmitOriginal(code, { 0xC5, 0xFA, 0x10, 0x40, 0x3C });
			code.L(complete);
		});
		SetAvailability(Feature::InfiniteFocus, focusAvailable);

		const auto arrowsBase = FindPattern("C3 48 8B 4B 30 8B 40 28 83");
		const bool arrowsAvailable = InstallMidHook("InfiniteArrowsAndTraps", arrowsBase ? arrowsBase + 1 : 0, 7,
			[](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original, complete;
			code.mov(rcx, qword[rbx + 0x30]);
			EmitFlagCheck(code, Feature::InfiniteArrowsAndTraps, original);
			code.mov(dword[rax + 0x28], 99);
			code.mov(eax, 99);
			code.jmp(complete);
			code.L(original);
			code.mov(eax, dword[rax + 0x28]);
			code.L(complete);
		});
		SetAvailability(Feature::InfiniteArrowsAndTraps, arrowsAvailable);

		const auto ignoreHits = FindPattern("48 8B F9 A9 00 00 00 A0 0F 85 ? ? 00 00");
		const bool ignoreHitsAvailable = InstallMidHook("IgnoreHits", ignoreHits, 8, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label originalTest, complete;
			code.mov(rdi, rcx);
			EmitFlagCheck(code, Feature::IgnoreHits, originalTest);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_playerEntity));
			code.cmp(qword[r11], rcx);
			code.pop(r11);
			code.jne(originalTest);
			code.test_(rcx, rcx);
			code.jmp(complete);
			code.L(originalTest);
			code.test_(eax, 0xA0000000);
			code.L(complete);
		});
		SetAvailability(Feature::IgnoreHits, ignoreHitsAvailable);

		const auto bowCharge = FindPattern("C5 F8 28 F1 C5 C0 57 FF 74 ? 80 ? ? ? 00 00 00 75");
		const bool bowChargeAvailable = InstallMidHook("InstantBowCharge", bowCharge, 8, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original;
			EmitFlagCheck(code, Feature::InstantBowCharge, original);
			code.push(r11);
			code.mov(r11d, std::bit_cast<uint32_t>(FullBowChargeValue));
			code.movd(xmm1, r11d);
			code.pop(r11);
			code.L(original);
			EmitOriginal(code, { 0xC5, 0xF8, 0x28, 0xF1, 0xC5, 0xC0, 0x57, 0xFF });
		});
		SetAvailability(Feature::InstantBowCharge, bowChargeAvailable);

		const auto damage = FindPattern("C5 CA 58 85 ? ? 00 00 C5 FA 11 85 ? ? 00 00 C5 CA 58 87 ? ? 00 00 C5 ? 11 ? ? ? 00 00");
		const auto damageDisplacement = damage ? ReadValue<int16_t>(damage + 4) : 0;
		const bool damageAvailable = InstallMidHook("DamageModifiers", damage, 8, [damageDisplacement](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label enemy, afterMultiplier, original;
			code.push(rax);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&OneValue));
			code.comiss(xmm6, dword[r11]);
			code.pop(r11);
			code.jbe(original);

			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_playerHealth));
			code.cmp(qword[rax], rdi);
			code.jne(enemy);
			code.mov(rax, FlagAddress(Feature::DefenseMultiplier));
			code.cmp(dword[rax], 0);
			code.je(original);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_defenseMultiplier));
			code.divss(xmm6, dword[rax]);
			code.jmp(original);

			code.L(enemy);
			code.mov(rax, FlagAddress(Feature::DamageMultiplier));
			code.cmp(dword[rax], 0);
			code.je(afterMultiplier);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_damageMultiplier));
			code.mulss(xmm6, dword[rax]);
			code.L(afterMultiplier);
			code.mov(rax, FlagAddress(Feature::SuperDamage));
			code.cmp(dword[rax], 0);
			code.je(original);
			code.mov(eax, std::bit_cast<uint32_t>(SuperDamageValue));
			code.movd(xmm6, eax);

			code.L(original);
			code.pop(rax);
			EmitOriginal(code, { 0xC5, 0xCA, 0x58, 0x85 });
			code.db(static_cast<uint8_t>(damageDisplacement & 0xFF));
			code.db(static_cast<uint8_t>((damageDisplacement >> 8) & 0xFF));
			EmitOriginal(code, { 0x00, 0x00 });
		});
		SetAvailability(Feature::SuperDamage, damageAvailable);
		SetAvailability(Feature::DamageMultiplier, damageAvailable);
		SetAvailability(Feature::DefenseMultiplier, damageAvailable);

		const auto experience = FindPattern("8B DA 48 8B F1 48 83 ? ? 00 0F 84 ? ? 00 00 ? 8B ? ? ? ? ? ? 85 ? 74");
		const bool experienceAvailable = InstallMidHook("Experience", experience, 5, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label skipMultiplier, skipGrant;
			EmitFlagCheck(code, Feature::ExperienceMultiplier, skipMultiplier);
			code.sub(rsp, 0x10);
			code.movdqu(ptr[rsp], xmm0);
			code.cvtsi2ss(xmm0, edx);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_experienceMultiplier));
			code.mulss(xmm0, dword[r11]);
			code.pop(r11);
			code.cvttss2si(edx, xmm0);
			code.movdqu(xmm0, ptr[rsp]);
			code.add(rsp, 0x10);
			code.L(skipMultiplier);

			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_grantExperience));
			code.cmp(dword[r11], 0);
			code.je(skipGrant);
			code.mov(dword[r11], 0);
			code.mov(edx, 999999);
			code.L(skipGrant);
			code.pop(r11);
			EmitOriginal(code, { 0x8B, 0xDA, 0x48, 0x8B, 0xF1 });
		});
		SetAvailability(Feature::ExperienceMultiplier, experienceAvailable);
		SetAvailability(Feature::GrantExperience, experienceAvailable);

		const auto skillPoints = FindPattern("8B 79 ? 48 8D 96 ? ? 00 00 E8 ? ? ? ? ? 0F B6");
		const auto skillPointOffset = skillPoints ? ReadValue<uint8_t>(skillPoints + 2) : 0;
		const auto skillPointLeaOffset = skillPoints ? ReadValue<uint16_t>(skillPoints + 7) : 0;
		const bool skillPointsAvailable = InstallMidHook("EditSkillPoints", skillPoints, 10,
			[skillPointOffset, skillPointLeaOffset](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original;
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_applySkillPoints));
			code.cmp(dword[r11], 0);
			code.je(original);
			code.mov(dword[r11], 0);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_skillPoints));
			code.mov(edi, dword[r11]);
			code.test_(edi, edi);
			code.jle(original);
			code.mov(dword[rcx + skillPointOffset], edi);
			code.L(original);
			code.pop(r11);
			code.mov(edi, dword[rcx + skillPointOffset]);
			code.lea(rdx, ptr[rsi + skillPointLeaOffset]);
		});
		SetAvailability(Feature::EditSkillPoints, skillPointsAvailable);

		const auto valor = FindPattern("C5 FA 10 81 ? ? 00 00 C5 F8 2F 40 10 0F 83 ? ? 00 00");
		const auto valorOffset = valor ? ReadValue<uint16_t>(valor + 4) : 0;
		const bool valorAvailable = InstallMidHook("InfiniteValor", valor, 8, [valorOffset](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original, store, complete;
			EmitFlagCheck(code, Feature::InfiniteValor, original);
			code.movss(xmm0, dword[rax + 0x10]);
			code.addss(xmm0, xmm0);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&MinimumValorValue));
			code.comiss(xmm0, dword[r11]);
			code.jae(store);
			code.movss(xmm0, dword[r11]);
			code.L(store);
			code.pop(r11);
			code.movss(dword[rcx + valorOffset], xmm0);
			code.jmp(complete);
			code.L(original);
			code.movss(xmm0, dword[rcx + valorOffset]);
			code.L(complete);
		});
		SetAvailability(Feature::InfiniteValor, valorAvailable);

		const auto itemType = FindPattern("83 ? FF 75 ? 48 8B 46 20 48 85 C0 74 ? 48 8B 08 48 8D 51 10 ? 85 ? 75");
		const auto itemTypeOffsetPattern = FindPattern("0F BE ? ? ? 00 00 ? 06 77 ? ? 4A 00 00 00 0F A3 ? 73");
		const auto itemTypeOffset = itemTypeOffsetPattern ? ReadValue<uint16_t>(itemTypeOffsetPattern + 3) : 0;
		const bool itemTypeAvailable = InstallMidHook("ItemTypeCapture", itemType ? itemType + 0xE : 0, 7,
			[itemTypeOffset](Xbyak::CodeGenerator& code)
		{
			code.movzx(edx, byte[rsi + itemTypeOffset]);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemType));
			code.mov(dword[r11], edx);
			code.pop(r11);
			code.mov(rcx, qword[rax]);
			code.lea(rdx, ptr[rcx + 0x10]);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemPointerCheck));
			code.mov(qword[r11], rdx);
			code.pop(r11);
		});

		const auto itemMaximumStack = itemType
			? FindPattern("44 8B 55 ? 4C 8B F0 ? 89 ? ? ? 89 ? ? 83 ? ? 02 75",
				itemType > 0x200 ? itemType - 0x200 : g_moduleBase, itemType)
			: 0;
		const auto maximumStackOffset = itemMaximumStack ? ReadValue<uint8_t>(itemMaximumStack + 3) : 0;
		const bool itemMaximumStackAvailable = InstallMidHook("ItemMaximumStackCapture", itemMaximumStack, 7,
			[maximumStackOffset](Xbyak::CodeGenerator& code)
		{
			code.mov(r10d, dword[rbp + maximumStackOffset]);
			code.mov(r14, rax);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemMaximumStack));
			code.mov(dword[r11], r10d);
			code.pop(r11);
		});

		const auto itemCheck = FindPattern("48 8B F1 33 FF 48 83 C1 60 E8 ? ? ? ? 83 ? FF 75 ? 8B");
		const bool itemCheckAvailable = InstallMidHook("PlayerItemCheck", itemCheck, 5, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original, accepted;
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_shouldEditItem));
			code.mov(dword[r11], 0);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_playerEntity));
			code.mov(rsi, qword[r11]);
			code.cmp(qword[rcx + 0x48], rsi);
			code.jne(original);

			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemMaximumStack));
			code.cmp(dword[r11], 1);
			code.mov(dword[r11], 0);
			code.jle(original);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemPointerCheck));
			code.cmp(qword[r11], rdx);
			code.mov(qword[r11], 0);
			code.jne(original);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_itemType));
			code.mov(esi, dword[r11]);
			code.cmp(esi, 1);
			code.je(accepted);
			code.cmp(esi, 3);
			code.je(accepted);
			code.cmp(esi, 6);
			code.jne(original);

			code.L(accepted);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_shouldEditItem));
			code.mov(dword[r11], 1);
			code.L(original);
			code.pop(r11);
			code.mov(rsi, rcx);
			code.xor_(edi, edi);
		});

		const auto itemEdit = itemCheck
			? FindPattern("03 79 50 8D 48 01 8D ? ? 23 ? ? 63", itemCheck, itemCheck + 0x200)
			: 0;
		const bool itemEditAvailable = InstallMidHook("PlayerItemEdit", itemEdit, 6, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label tools, ammo, resources, writeValue, restore;
			code.push(rax);
			code.push(rdx);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_shouldEditItem));
			code.cmp(dword[rax], 1);
			code.jne(restore);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_itemType));
			code.mov(edx, dword[rax]);
			code.cmp(edx, 1);
			code.je(tools);
			code.cmp(edx, 3);
			code.je(ammo);
			code.cmp(edx, 6);
			code.je(resources);
			code.jmp(restore);

			code.L(tools);
			code.mov(rax, FlagAddress(Feature::EditTools));
			code.cmp(dword[rax], 0);
			code.je(restore);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_toolsAmount));
			code.jmp(writeValue);

			code.L(ammo);
			code.mov(rax, FlagAddress(Feature::EditAmmo));
			code.cmp(dword[rax], 0);
			code.je(restore);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_ammoAmount));
			code.jmp(writeValue);

			code.L(resources);
			code.mov(rax, FlagAddress(Feature::EditResources));
			code.cmp(dword[rax], 0);
			code.je(restore);
			code.mov(rax, reinterpret_cast<std::uintptr_t>(&g_resourcesAmount));
			code.L(writeValue);
			code.mov(edx, dword[rax]);
			code.test_(edx, edx);
			code.jle(restore);
			code.mov(dword[rcx + 0x50], edx);

			code.L(restore);
			code.pop(rdx);
			code.pop(rax);
			code.add(edi, dword[rcx + 0x50]);
			code.lea(ecx, ptr[rax + 1]);
		});

		const bool itemEditingAvailable = itemTypeAvailable && itemMaximumStackAvailable && itemCheckAvailable && itemEditAvailable;
		SetAvailability(Feature::EditTools, itemEditingAvailable);
		SetAvailability(Feature::EditAmmo, itemEditingAvailable);
		SetAvailability(Feature::EditResources, itemEditingAvailable);

		// Remaining compound hooks are installed below. Stealth requires both its hook and the AI branch patch.
		const auto stealth = FindPattern("80 7B 20 00 48 8D 93 ? ? 00 00 74 ? C5 ? 10 ? ? ? 00 00 C5");
		const auto stealthOffset = stealth ? ReadValue<uint16_t>(stealth + 7) : 0;
		const bool stealthHookAvailable = InstallMidHook("StealthMode", stealth, 11, [stealthOffset](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original;
			EmitFlagCheck(code, Feature::StealthMode, original);
			code.mov(byte[rbx + 0x20], 0);
			code.L(original);
			code.cmp(byte[rbx + 0x20], 0);
			code.lea(rdx, ptr[rbx + stealthOffset]);
		});
		SetAvailability(Feature::StealthMode, stealthHookAvailable && g_disableAIPatch.Address != 0);

		const auto trialTimer = FindPattern("C5 FB 5C 89 98 00 00 00 C5 ? 5A ? C5 ? 5F ? C3");
		const bool trialTimerAvailable = InstallMidHook("FreezeTrialTimer", trialTimer, 8, [](Xbyak::CodeGenerator& code)
		{
			Xbyak::Label original;
			EmitFlagCheck(code, Feature::FreezeTrialTimer, original);
			code.movsd(qword[rcx + 0x98], xmm0);
			code.L(original);
			EmitOriginal(code, { 0xC5, 0xFB, 0x5C, 0x89, 0x98, 0x00, 0x00, 0x00 });
		});
		SetAvailability(Feature::FreezeTrialTimer, trialTimerAvailable);

		const auto waypoint = FindPattern("C5 FB 11 41 20 ? 89 ? 28 ? 89 ? 30 FF");
		const bool waypointAvailable = InstallMidHook("TeleportToWaypoint", waypoint, 5, [](Xbyak::CodeGenerator& code)
		{
			code.push(rax);
			code.push(r11);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_waypointX));
			code.mov(rax, qword[rcx + 0x10]);
			code.mov(qword[r11], rax);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_waypointY));
			code.mov(rax, qword[rcx + 0x18]);
			code.mov(qword[r11], rax);
			code.mov(r11, reinterpret_cast<std::uintptr_t>(&g_waypointZ));
			code.movsd(qword[r11], xmm0);
			code.pop(r11);
			code.pop(rax);
			EmitOriginal(code, { 0xC5, 0xFB, 0x11, 0x41, 0x20 });
		});
		SetAvailability(Feature::TeleportToWaypoint, waypointAvailable);
	}
}
