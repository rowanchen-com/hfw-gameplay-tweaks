#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <shared_mutex>
#include <utility>
#include <imgui.h>
#include <Windows.h>
#include "../../ModConfiguration.h"
#include "../../ModCoreEvents.h"
#include "../Core/DebugSettings.h"
#include "../Core/Destructibility.h"
#include "../Core/GameModule.h"
#include "../Core/GameWorldTimeState.h"
#include "../Core/JobHeaderCPU.h"
#include "../Core/Mover.h"
#include "../Core/PlayerGame.h"
#include "../Core/RTTIRefObject.h"
#include "../Core/RTTIYamlExporter.h"
#include "../RTTIScanner.h"
#include "../TrainerCheats.h"
#include "DebugUI.h"
#include "DemoWindow.h"
#include "EntitySpawnerWindow.h"
#include "LogWindow.h"
#include "MainMenuBar.h"
#include "PlayerInventoryWindow.h"
#include "TrainerValueEditorWindow.h"
#include "WeatherSetupWindow.h"

namespace
{
	struct TeleportEntry
	{
		const char *Name;
		HRZ2::WorldPosition Position;
	};

	const std::array TeleportLocations = {
		TeleportEntry { "HZD - 子午城入口", { 3918.612, 5465.897, 830.652 } },
		TeleportEntry { "HZD - 尖塔", { 4162.499, 4757.230, 774.453 } },
		TeleportEntry { "HZD - 庄园", { 4082.374, 4614.900, 710.106 } },
		TeleportEntry { "HZD - 门地", { 4695.546, 5482.825, 787.018 } },
		TeleportEntry { "HZD - 孤光", { 4772.899, 5777.062, 780.938 } },
		TeleportEntry { "HZD - 炽焰拱门", { 3086.388, 5931.355, 813.258 } },
		TeleportEntry { "HZD - 炼铸厂 ZETA", { 4050.798, 6724.117, 840.551 } },
		TeleportEntry { "HZD - 日落之地竞技场", { 3009.000, 6565.725, 868.025 } },
		TeleportEntry { "HZD - 造者末途", { 3435.481, 7210.677, 825.362 } },
		TeleportEntry { "HZD - 丹特", { 1662.439, 5811.224, 794.055 } },
		TeleportEntry { "HZD - 耀光峡谷", { 5260.463, 6411.703, 820.775 } },
		TeleportEntry { "HFW - 开场过场动画 1", { 5737.400, -2394.600, 432.400 } },
		TeleportEntry { "HFW - 开场过场动画 2", { 6315.800, -1698.400, 320.100 } },
		TeleportEntry { "HFW - 开场过场动画 3", { 6942.200, -1713.900, 321.000 } },
		TeleportEntry { "HFW - 开场过场动画 4（树之梦）", { 2691.584, -3752.680, 477.490 } },
		TeleportEntry { "HFW - 序章教学区域", { 5645.362, -2886.453, 403.551 } },
		TeleportEntry { "HFW - 序章法尔·泽尼斯设施", { 5993.999, -2915.253, 334.978 } },
		TeleportEntry { "HFW - 序章法尔·泽尼斯航天飞机", { 6465.844, -3147.694, 331.482 } },
		TeleportEntry { "HFW - 贫瘠之光山地要塞", { 2690.644, 441.522, 677.341 } },
		TeleportEntry { "HFW - 炙炎海岸首领战区域", { -140.415, -4405.686, 291.182 } },
		TeleportEntry { "HFW - 炙炎海岸未完成区域", { 2107.752, -6777.761, 302.759 } },
		TeleportEntry { "HFW - 顶级猎杀：西浅滩", { -4531.792, -460.949, 185.629 } },
		TeleportEntry { "HFW - 顶级猎杀：灰峰", { -1452.500, -614.100, 520.100 } },
		TeleportEntry { "HFW - 法尔·泽尼斯基地", { -1417.900, -3088.700, 283.900 } },
	};

	bool IsNavigationKeyPressed(ImGuiKey Primary, ImGuiKey Alternate)
	{
		return ImGui::IsKeyPressed(Primary) || ImGui::IsKeyPressed(Alternate);
	}
}

namespace HRZ2::DebugUI
{
	MainMenuBar::MainMenuBar()
	{
		m_LODRangeModifier = ModCoreEvents::GetInstance().m_CachedLODRangeModifierHack;
		m_EnableGodMode = ModConfiguration.EnableGodMode;
		m_EnableInfiniteClipAmmo = ModConfiguration.EnableInfiniteClipAmmo;
		m_EnableAutoNeutralFaction = ModConfiguration.EnableAutoNeutralFaction;
	}

	void MainMenuBar::Render()
	{
		if (!m_IsVisible)
			return;

		auto items = BuildMenuItems();
		if (items.empty())
			items.emplace_back(MenuItem { .Label = "暂无可用选项", .Description = "当前游戏状态下没有可用功能。", .Enabled = false });

		const bool stateChanged = HandleMenuInput(items);
		if (!m_IsVisible)
			return;

		if (stateChanged)
		{
			items = BuildMenuItems();
			if (items.empty())
				items.emplace_back(MenuItem { .Label = "暂无可用选项", .Description = "当前游戏状态下没有可用功能。", .Enabled = false });
		}

		DrawTrainerFrame(items);
	}

	bool MainMenuBar::Close()
	{
		return false;
	}

	std::string MainMenuBar::GetId() const
	{
		return "HFW Trainer Menu";
	}

	std::vector<MainMenuBar::MenuItem> MainMenuBar::BuildMenuItems()
	{
		std::vector<MenuItem> items;

		auto addAction = [&](std::string Label, std::string Description, std::function<void()> Callback, bool Enabled = true)
		{
			items.emplace_back(MenuItem {
				.Label = std::move(Label), .Value = "执行", .Description = std::move(Description),
				.Enabled = Enabled, .Activate = std::move(Callback),
			});
		};

		auto addSubmenu = [&](std::string Label, std::string Description, Page Target, bool Enabled = true)
		{
			items.emplace_back(MenuItem {
				.Label = std::move(Label), .Value = ">", .Description = std::move(Description),
				.Enabled = Enabled, .IsSubmenu = true, .Activate = [this, Target]() { OpenPage(Target); },
			});
		};

		auto addToolWindow = [&](std::string Label, std::string Description, std::function<void()> Callback, bool Enabled = true)
		{
			addAction(std::move(Label), std::move(Description), std::move(Callback), Enabled);
			items.back().Value = "打开";
		};

		auto addToggle = [&items](std::string Label, std::string Description, bool CurrentValue,
			std::function<void(bool)> Setter, bool Enabled = true)
		{
			items.emplace_back(MenuItem {
				.Label = std::move(Label), .Value = CurrentValue ? "开启" : "关闭", .Description = std::move(Description),
				.Enabled = Enabled,
				.Activate = [CurrentValue, Setter]() { Setter(!CurrentValue); },
				.AdjustLeft = [Setter]() { Setter(false); }, .AdjustRight = [Setter]() { Setter(true); },
			});
		};

		auto addValue = [&items](std::string Label, std::string Value, std::string Description,
			std::function<void()> AdjustLeft, std::function<void()> AdjustRight, bool Enabled = true)
		{
			items.emplace_back(MenuItem {
				.Label = std::move(Label), .Value = std::move(Value), .Description = std::move(Description),
				.Enabled = Enabled, .AdjustLeft = std::move(AdjustLeft), .AdjustRight = std::move(AdjustRight),
			});
		};

		auto featureDescription = [](TrainerCheats::Feature FeatureValue, std::string Description)
		{
			if (!TrainerCheats::IsAvailable(FeatureValue))
			{
				Description += "（不可用：";
				Description += TrainerCheats::GetUnavailableReason(FeatureValue);
				Description += "）";
			}
			return Description;
		};

		auto addFeatureToggle = [&](std::string Label, std::string Description, TrainerCheats::Feature FeatureValue)
		{
			addToggle(std::move(Label), featureDescription(FeatureValue, std::move(Description)),
				TrainerCheats::IsEnabled(FeatureValue), [FeatureValue](bool Enabled)
				{
					TrainerCheats::SetEnabled(FeatureValue, Enabled);
				}, TrainerCheats::IsAvailable(FeatureValue));
		};

		auto addValueEditor = [&](std::string Label, std::string Value, std::string Description,
			TrainerValueEditorWindow::Mode EditorMode, TrainerCheats::Feature FeatureValue)
		{
			items.emplace_back(MenuItem {
				.Label = std::move(Label), .Value = std::move(Value),
				.Description = featureDescription(FeatureValue, std::move(Description)),
				.Enabled = TrainerCheats::IsAvailable(FeatureValue), .IsSubmenu = true,
				.Activate = [EditorMode]() { AddWindow(std::make_shared<TrainerValueEditorWindow>(EditorMode)); },
			});
		};

		const auto currentPage = m_Navigation.back().PageId;
		auto player = Player::GetLocalPlayer();
		const bool playerAvailable = player && player->m_Entity;

		switch (currentPage)
		{
		case Page::Home:
			addSubmenu("玩家选项", "自由镜头、穿墙、原有引擎无敌、弹药和玩家状态。", Page::Player, playerAvailable);
			addSubmenu("战斗强化", "无视命中、生命、伤害、弹药与弓箭相关功能。", Page::Combat, playerAvailable);
			addSubmenu("生存与技能", "专注、勇气、氧气、技能持续时间及潜行功能。", Page::Survival, playerAvailable);
			addSubmenu("资源与成长", "物品数量、制作、经验与技能点功能。", Page::Resources, playerAvailable);
			addSubmenu("世界与时间", "控制游戏暂停、昼夜时间、时间倍率和显示距离。", Page::World);
			addSubmenu("传送", "将玩家传送到预设坐标或自由镜头位置。", Page::Teleport, playerAvailable);
			addSubmenu("玩家阵营", "切换玩家使用的游戏内部 AI 阵营。", Page::Faction, playerAvailable);
			addToolWindow("玩家物品栏", "打开物品管理窗口；操作任务物品前请先备份存档。", []() { AddWindow(std::make_shared<PlayerInventoryWindow>()); }, playerAvailable);
			addToolWindow("实体生成器", "打开实体生成窗口，可生成机器、动物和其他实体。", []() { AddWindow(std::make_shared<EntitySpawnerWindow>()); }, playerAvailable);
			addToolWindow("天气设置", "打开天气资源选择窗口。", []() { AddWindow(std::make_shared<WeatherSetupWindow>()); });
			addSubmenu("实用工具", "保存、读取和菜单控制。", Page::Utilities);
			addSubmenu("开发者工具", "日志、RTTI 导出和调试功能。", Page::Developer);
			break;

		case Page::Player:
		{
			auto debugSettings = DebugSettings::GetInstance();
			const bool debugAvailable = debugSettings != nullptr;

			addToggle("穿墙模式", "让埃洛伊脱离碰撞并使用自由移动。", m_FreeCamMode == FreeCamMode::Noclip, [](bool Enabled)
			{
				if ((m_FreeCamMode == FreeCamMode::Noclip) != Enabled) ToggleNoclip();
			}, playerAvailable);
			addToggle("自由镜头", "让镜头脱离玩家移动；按住鼠标右键旋转。", m_FreeCamMode == FreeCamMode::Free, [](bool Enabled)
			{
				if ((m_FreeCamMode == FreeCamMode::Free) != Enabled) ToggleFreeflyCamera();
			}, playerAvailable);
			addToggle("无敌模式", "免疫伤害，同时保留正常生命值逻辑。", m_EnableGodMode, [](bool Enabled)
			{
				if (auto p = Player::GetLocalPlayer(); p && p->m_Entity && p->m_Entity->m_Destructibility)
				{
					auto d = p->m_Entity->m_Destructibility;
					m_EnableGodMode = Enabled;
					if (Enabled) m_EnableDemigodMode = false;
					d->m_Invulnerable = Enabled;
					d->m_DieAtZeroHealth = true;
				}
			}, playerAvailable);
			addToggle("半无敌模式", "仍会受到伤害，但生命值降至零时不会死亡。", m_EnableDemigodMode, [](bool Enabled)
			{
				if (auto p = Player::GetLocalPlayer(); p && p->m_Entity && p->m_Entity->m_Destructibility)
				{
					auto d = p->m_Entity->m_Destructibility;
					m_EnableDemigodMode = Enabled;
					if (Enabled) m_EnableGodMode = false;
					d->m_Invulnerable = false;
					d->m_DieAtZeroHealth = !Enabled;
				}
			}, playerAvailable);
			addToggle("无限备用弹药", "射击时不消耗物品栏中的备用弹药。", debugAvailable && debugSettings->m_InfiniteAmmo, [](bool Enabled)
			{
				if (auto s = DebugSettings::GetInstance())
				{
					s->m_InfiniteAmmo = Enabled;
					if (Enabled) { s->m_InfiniteSizeClip = false; m_EnableInfiniteClipAmmo = false; }
				}
			}, debugAvailable);
			addToggle("无限弹匣弹药", "当前武器无需重新装填，并与无限备用弹药互斥。", m_EnableInfiniteClipAmmo, [](bool Enabled)
			{
				if (auto s = DebugSettings::GetInstance())
				{
					m_EnableInfiniteClipAmmo = Enabled;
					s->m_InfiniteSizeClip = Enabled;
					if (Enabled) s->m_InfiniteAmmo = false;
				}
			}, debugAvailable);
			addToggle("无限武器耐力", "武器相关耐力不会耗尽。", debugAvailable && debugSettings->m_Inexhaustible, [](bool Enabled)
			{
				if (auto s = DebugSettings::GetInstance()) s->m_Inexhaustible = Enabled;
			}, debugAvailable);
			addToggle("自动中立阵营", "持续将玩家阵营设置为中立；手动选阵营会关闭此项。", m_EnableAutoNeutralFaction,
				[](bool Enabled) { m_EnableAutoNeutralFaction = Enabled; }, playerAvailable);
			addToggle("模拟游戏已完成", "解锁依赖通关状态的调试内容。", debugAvailable && debugSettings->m_SPAllUnlocked, [](bool Enabled)
			{
				if (auto s = DebugSettings::GetInstance()) s->m_SPAllUnlocked = Enabled;
			}, debugAvailable);
			addToggle("游戏中应用拍照模式设置", "无需进入拍照模式即可应用部分拍照参数。",
				debugAvailable && debugSettings->m_ApplyPhotoModeSettingsIngame, [](bool Enabled)
			{
				if (auto s = DebugSettings::GetInstance()) s->m_ApplyPhotoModeSettingsIngame = Enabled;
			}, debugAvailable);
			break;
		}

		case Page::Combat:
		{
			addFeatureToggle("无视命中判定", "让针对玩家的伤害命中判定直接失效；与原有引擎无敌是两套独立实现。",
				TrainerCheats::Feature::IgnoreHits);
			addFeatureToggle("无限生命", "持续将玩家生命值保持为 9999。", TrainerCheats::Feature::InfiniteHealth);
			addFeatureToggle("超级伤害 / 一击必杀", "把对敌人的有效伤害提高到极高数值；部分特殊目标可能仍有剧情保护。",
				TrainerCheats::Feature::SuperDamage);
			addValueEditor("伤害倍率设置", TrainerCheats::IsEnabled(TrainerCheats::Feature::DamageMultiplier)
				? std::format("{:.1f}x", TrainerCheats::GetDamageMultiplier()) : "关闭",
				"打开二级窗口输入倍率；只有确认后才启用。", TrainerValueEditorWindow::Mode::DamageMultiplier,
				TrainerCheats::Feature::DamageMultiplier);
			addValueEditor("防御倍率设置", TrainerCheats::IsEnabled(TrainerCheats::Feature::DefenseMultiplier)
				? std::format("{:.1f}x", TrainerCheats::GetDefenseMultiplier()) : "关闭",
				"打开二级窗口输入倍率；只有确认后才启用。", TrainerValueEditorWindow::Mode::DefenseMultiplier,
				TrainerCheats::Feature::DefenseMultiplier);
			addFeatureToggle("无限箭矢与陷阱", "使用箭矢或陷阱时把当前计数保持为 99。",
				TrainerCheats::Feature::InfiniteArrowsAndTraps);
			addFeatureToggle("弓箭瞬间蓄力", "拉弓时立即达到完整蓄力。", TrainerCheats::Feature::InstantBowCharge);
			break;
		}

		case Page::Survival:
			addFeatureToggle("无限专注", "持续恢复武器轮盘与瞄准使用的专注值。", TrainerCheats::Feature::InfiniteFocus);
			addFeatureToggle("无限勇气", "持续补充勇气激增所需的勇气值。", TrainerCheats::Feature::InfiniteValor);
			addFeatureToggle("无限技能持续时间", "冻结受支持技能的剩余持续时间。", TrainerCheats::Feature::InfiniteSkillDuration);
			addFeatureToggle("无限氧气", "水下活动时不再消耗氧气。", TrainerCheats::Feature::InfiniteOxygen);
			addFeatureToggle("药用浆果袋保持满额", "消耗药用浆果后立即恢复到当前容量。", TrainerCheats::Feature::MaxMedicinePouch);
			addFeatureToggle("隐身模式", "同时关闭相关 AI 发现分支并持续清除警觉状态。", TrainerCheats::Feature::StealthMode);
			addFeatureToggle("锁定试炼时间", "冻结狩猎场等受支持试炼的计时器。", TrainerCheats::Feature::FreezeTrialTimer);
			break;

		case Page::Resources:
		{
			const auto addItemEditor = [&](const char *Name, TrainerCheats::Feature FeatureValue,
				TrainerValueEditorWindow::Mode EditorMode)
			{
				addValueEditor(std::string("修改") + Name + "数量",
					TrainerCheats::IsEnabled(FeatureValue) ? "等待应用" : "未启用",
					std::string("打开二级窗口输入") + Name + "数量；默认不修改，只执行一次。", EditorMode, FeatureValue);
			};

			addItemEditor("工具", TrainerCheats::Feature::EditTools, TrainerValueEditorWindow::Mode::ToolsAmount);
			addItemEditor("弹药", TrainerCheats::Feature::EditAmmo, TrainerValueEditorWindow::Mode::AmmoAmount);
			addItemEditor("资源", TrainerCheats::Feature::EditResources, TrainerValueEditorWindow::Mode::ResourcesAmount);
			addFeatureToggle("无视制作与购买需求", "运行时跳过材料和购买条件检查。",
				TrainerCheats::Feature::IgnoreCraftingRequirements);
			addValueEditor("经验倍率设置", TrainerCheats::IsEnabled(TrainerCheats::Feature::ExperienceMultiplier)
				? std::format("{:.1f}x", TrainerCheats::GetExperienceMultiplier()) : "关闭",
				"打开二级窗口输入倍率；只有确认后才启用。", TrainerValueEditorWindow::Mode::ExperienceMultiplier,
				TrainerCheats::Feature::ExperienceMultiplier);
			addAction("获得大量经验", featureDescription(TrainerCheats::Feature::GrantExperience,
				"排队一次 999999 经验修改，并在下一次经验结算时生效。"), []() { TrainerCheats::GrantExperience(); },
				TrainerCheats::IsAvailable(TrainerCheats::Feature::GrantExperience));
			addValueEditor("修改技能点", "未启用", "打开二级窗口输入技能点；确认后只执行一次。",
				TrainerValueEditorWindow::Mode::SkillPoints, TrainerCheats::Feature::EditSkillPoints);
			break;
		}

		case Page::World:
		{
			addToggle("暂停游戏逻辑", "暂停大部分游戏世界更新。", m_PauseGame, [](bool Enabled) { m_PauseGame = Enabled; });
			addToggle("暂停 AI 处理", "暂停敌人与其他 AI 的更新。", m_PauseAIProcessing, [](bool Enabled) { m_PauseAIProcessing = Enabled; });

			auto module = GameModule::GetInstance();
			auto time = module ? module->m_WorldTimeState : nullptr;
			const bool timeAvailable = time != nullptr;
			addToggle("暂停昼夜时间", "冻结当前时刻。", timeAvailable && time->m_IsPaused, [](bool Enabled)
			{
				JobHeaderCPU::SubmitCallable([Enabled]()
				{
					if (auto m = GameModule::GetInstance(); m && m->m_WorldTimeState) m->m_WorldTimeState->m_IsPaused = Enabled;
				});
			}, timeAvailable);
			addToggle("启用昼夜循环", "允许游戏时间正常推进。", timeAvailable && time->m_EnableDayNightCycle, [](bool Enabled)
			{
				if (auto m = GameModule::GetInstance(); m && m->m_WorldTimeState) m->m_WorldTimeState->m_EnableDayNightCycle = Enabled;
			}, timeAvailable);
			addValue("当前时间", timeAvailable ? std::format("{:.1f} 时", time->m_TimeOfDay) : "不可用",
				"使用左右键以半小时为单位调整昼夜时间。", []()
			{
				if (auto m = GameModule::GetInstance(); m && m->m_WorldTimeState)
				{
					auto value = m->m_WorldTimeState->m_TimeOfDay - 0.5f;
					if (value < 0.0f) value += 24.0f;
					m->m_WorldTimeState->SetTimeOfDay(value, 0.0f);
				}
			}, []()
			{
				if (auto m = GameModule::GetInstance(); m && m->m_WorldTimeState)
				{
					auto value = std::fmod(m->m_WorldTimeState->m_TimeOfDay + 0.5f, 24.0f);
					m->m_WorldTimeState->SetTimeOfDay(value, 0.0f);
				}
			}, timeAvailable);
			addToggle("时间倍率覆盖", "使用自定义倍率覆盖正常游戏速度。", m_TimescaleOverride,
				[](bool Enabled) { m_TimescaleOverride = Enabled; });
			addToggle("菜单内保持时间倍率", "打开游戏内菜单时仍应用自定义时间倍率。", m_TimescaleOverrideInMenus,
				[](bool Enabled) { m_TimescaleOverrideInMenus = Enabled; });
			addValue("时间倍率", std::format("{:.2f}x", m_Timescale), "左右键调整；确认键恢复 1.00x。",
				[]() { AdjustTimescale(-0.25f); }, []() { AdjustTimescale(0.25f); });
			items.back().Activate = []() { m_Timescale = 1.0f; m_TimescaleOverride = true; };

			const bool lodEnabled = m_LODRangeModifier != std::numeric_limits<float>::max();
			addToggle("LOD 偏差覆盖", "覆盖游戏视图的细节层级距离倍率。", lodEnabled, [](bool Enabled)
			{
				m_LODRangeModifier = Enabled ? 1.0f : std::numeric_limits<float>::max();
			});
			addValue("LOD 偏差", lodEnabled ? std::format("{:.2f}", m_LODRangeModifier) : "未启用", "使用左右键调整 LOD 距离倍率。",
				[]() { if (m_LODRangeModifier != std::numeric_limits<float>::max()) m_LODRangeModifier = std::clamp(m_LODRangeModifier - 0.05f, 0.0f, 1.0f); },
				[]() { if (m_LODRangeModifier != std::numeric_limits<float>::max()) m_LODRangeModifier = std::clamp(m_LODRangeModifier + 0.05f, 0.0f, 1.0f); }, lodEnabled);
			break;
		}

		case Page::Teleport:
			addAction("保存当前位置", "记录玩家当前世界坐标；仅保存在本次游戏进程中。", []()
			{
				if (auto p = Player::GetLocalPlayer(); p && p->m_Entity)
					m_SavedPosition = p->m_Entity->GetWorldTransform().Position;
			}, playerAvailable);
			addAction("传送到保存位置", m_SavedPosition
				? std::format("坐标：{:.1f}, {:.1f}, {:.1f}", m_SavedPosition->X, m_SavedPosition->Y, m_SavedPosition->Z)
				: "尚未保存位置。", []() { if (m_SavedPosition) TeleportTo(*m_SavedPosition); }, playerAvailable && m_SavedPosition.has_value());
			addAction("撤销上次传送", m_UndoPosition
				? std::format("返回：{:.1f}, {:.1f}, {:.1f}", m_UndoPosition->X, m_UndoPosition->Y, m_UndoPosition->Z)
				: "本次游戏进程中尚未执行传送。", []() { if (m_UndoPosition) TeleportTo(*m_UndoPosition); },
				playerAvailable && m_UndoPosition.has_value());
			{
				const auto waypoint = TrainerCheats::GetWaypointPosition();
				addAction("传送到地图标记点", waypoint
					? std::format("坐标：{:.1f}, {:.1f}, {:.1f}", waypoint->X, waypoint->Y, waypoint->Z)
					: featureDescription(TrainerCheats::Feature::TeleportToWaypoint, "尚未读取到有效地图标记点。"), []()
				{
					if (const auto target = TrainerCheats::GetWaypointPosition()) TeleportTo(*target);
				}, playerAvailable && waypoint.has_value());
			}
			addAction("自由镜头位置", std::format("坐标：{:.1f}, {:.1f}, {:.1f}", m_FreeCamPosition.Position.X,
				m_FreeCamPosition.Position.Y, m_FreeCamPosition.Position.Z), []() { TeleportTo(m_FreeCamPosition.Position); }, playerAvailable);
			for (const auto& location : TeleportLocations)
				addAction(location.Name, std::format("坐标：{:.1f}, {:.1f}, {:.1f}", location.Position.X, location.Position.Y, location.Position.Z),
					[position = location.Position]() { TeleportTo(position); }, playerAvailable);
			break;

		case Page::Faction:
		{
			addToggle("自动中立阵营", "持续把玩家恢复为中立阵营。", m_EnableAutoNeutralFaction,
				[](bool Enabled) { m_EnableAutoNeutralFaction = Enabled; }, playerAvailable);
			struct FactionEntry { std::string Name; Ref<RTTIRefObject> Object; };
			std::vector<FactionEntry> factions;
			auto& events = ModCoreEvents::GetInstance();
			{
				std::shared_lock lock(events.m_CachedDataMutex);
				factions.reserve(events.m_CachedAIFactions.size());
				for (auto faction : events.m_CachedAIFactions)
				{
					const auto name = faction->GetMemberRefUnsafe<String>("Name");
					factions.emplace_back(FactionEntry { std::string(name.data(), name.size()), faction });
				}
			}
			std::ranges::sort(factions, {}, &FactionEntry::Name);
			auto currentFaction = playerAvailable ? reinterpret_cast<RTTIRefObject *>(player->m_Entity->m_Faction) : nullptr;
			for (const auto& faction : factions)
			{
				items.emplace_back(MenuItem {
					.Label = faction.Name, .Value = faction.Object.GetPtr() == currentFaction ? "当前" : "",
					.Description = "游戏内部阵营标识；保持英文可避免资源名称歧义。", .Enabled = playerAvailable,
					.Activate = [factionRef = faction.Object]()
					{
						m_EnableAutoNeutralFaction = false;
						JobHeaderCPU::SubmitCallable([factionRef]()
						{
							if (auto p = Player::GetLocalPlayer(); p && p->m_Entity)
								p->m_Entity->SetFaction(reinterpret_cast<AIFaction *>(factionRef.GetPtr()));
						});
					},
				});
			}
			break;
		}

		case Page::Utilities:
			addAction("强制快速保存", "立即请求一次快速保存。", []() { ToggleQuickSave(); }, playerAvailable);
			addAction("读取上一存档", "立即读取最近一次存档。", []() { ToggleQuickLoad(); }, playerAvailable);
			addToolWindow("玩家物品栏", "打开物品管理窗口。", []() { AddWindow(std::make_shared<PlayerInventoryWindow>()); }, playerAvailable);
			addToolWindow("实体生成器", "打开实体生成窗口。", []() { AddWindow(std::make_shared<EntitySpawnerWindow>()); }, playerAvailable);
			addToolWindow("天气设置", "打开天气资源选择窗口。", []() { AddWindow(std::make_shared<WeatherSetupWindow>()); });
			addAction("关闭修改器菜单", "关闭菜单并恢复游戏输入。", []() { SetMenuVisible(false); });
			addSubmenu("结束游戏", "显示确认页面后终止当前游戏进程。", Page::ConfirmExit);
			break;

		case Page::Developer:
			addToolWindow("显示日志窗口", "打开模组内部日志窗口。", []() { AddWindow(std::make_shared<LogWindow>()); });
			addToolWindow("显示 ImGui 演示窗口", "打开 Dear ImGui 官方英文演示窗口。", []() { AddWindow(std::make_shared<DemoWindow>()); });
			addAction("导出 RTTI 结构", "将已扫描的 RTTI 类型导出到游戏目录。", []()
			{
				RTTIYamlExporter exporter(RTTIScanner::GetAllTypes());
				exporter.ExportRTTITypes(".");
			}, !RTTIScanner::GetAllTypes().empty());
			addAction("导出玩家组件", "将玩家实体及组件信息写入日志。", []() { DumpPlayerComponents(); }, playerAvailable);
			items.emplace_back(MenuItem { .Label = "项目版本", .Value = "0.18 中文版", .Description = "HFW Gameplay Tweaks；原作者 Nukem。", .Enabled = false });
			break;

		case Page::ConfirmExit:
			addAction("取消", "返回上一页，不结束游戏。", [this]() { GoBack(); });
			addAction("确认结束游戏", "立即终止游戏进程；未保存进度会丢失。", []() { TerminateProcess(GetCurrentProcess(), 0); });
			break;
		}

		return items;
	}

	bool MainMenuBar::HandleMenuInput(std::vector<MenuItem>& Items)
	{
		if (Items.empty())
			return false;
		if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive())
			return false;

		auto& state = m_Navigation.back();
		state.SelectedIndex = std::min(state.SelectedIndex, Items.size() - 1);

		if (IsNavigationKeyPressed(ImGuiKey_UpArrow, ImGuiKey_W))
		{
			state.SelectedIndex = state.SelectedIndex == 0 ? Items.size() - 1 : state.SelectedIndex - 1;
			return false;
		}

		if (IsNavigationKeyPressed(ImGuiKey_DownArrow, ImGuiKey_S))
		{
			state.SelectedIndex = (state.SelectedIndex + 1) % Items.size();
			return false;
		}

		if (IsNavigationKeyPressed(ImGuiKey_LeftArrow, ImGuiKey_A))
		{
			auto& item = Items[state.SelectedIndex];
			if (item.Enabled && item.AdjustLeft)
				item.AdjustLeft();
			return true;
		}

		if (IsNavigationKeyPressed(ImGuiKey_RightArrow, ImGuiKey_D))
		{
			auto& item = Items[state.SelectedIndex];
			if (item.Enabled)
			{
				if (item.AdjustRight)
					item.AdjustRight();
				else if (item.IsSubmenu)
					ActivateItem(item);
			}
			return true;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
		{
			ActivateItem(Items[state.SelectedIndex]);
			return true;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			GoBack();
			return true;
		}

		return false;
	}

	void MainMenuBar::DrawTrainerFrame(std::vector<MenuItem>& Items)
	{
		auto& io = ImGui::GetIO();
		auto& state = m_Navigation.back();
		state.SelectedIndex = std::min(state.SelectedIndex, Items.size() - 1);

		const float scale = std::clamp(io.DisplaySize.y / 1080.0f, 0.85f, 1.50f);
		const float width = std::min(520.0f * scale, io.DisplaySize.x - 36.0f * scale);
		const float headerHeight = 96.0f * scale;
		const float breadcrumbHeight = 42.0f * scale;
		const float rowHeight = 43.0f * scale;
		const float footerHeight = 150.0f * scale;
		const size_t maximumVisibleRows = 12;
		const float fixedHeight = headerHeight + breadcrumbHeight + footerHeight;
		const float availableRowsHeight = std::max(rowHeight, io.DisplaySize.y - 36.0f * scale - fixedHeight);
		const size_t rowsAllowedByDisplay = std::max<size_t>(1, static_cast<size_t>(availableRowsHeight / rowHeight));
		const size_t visibleRows = std::min({ maximumVisibleRows, Items.size(), rowsAllowedByDisplay });
		const float height = headerHeight + breadcrumbHeight + rowHeight * visibleRows + footerHeight;

		if (state.SelectedIndex < state.ScrollOffset)
			state.ScrollOffset = state.SelectedIndex;
		else if (state.SelectedIndex >= state.ScrollOffset + visibleRows)
			state.ScrollOffset = state.SelectedIndex - visibleRows + 1;

		const auto maximumScroll = Items.size() > visibleRows ? Items.size() - visibleRows : 0;
		state.ScrollOffset = std::min(state.ScrollOffset, maximumScroll);

		const ImVec2 position(
			std::max(18.0f * scale, std::min(34.0f * scale, io.DisplaySize.x - width - 18.0f * scale)),
			std::max(18.0f * scale, std::min(72.0f * scale, io.DisplaySize.y - height - 18.0f * scale)));

		ImGui::SetNextWindowPos(position, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, 1.25f * scale));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.030f, 0.038f, 0.995f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.78f, 0.83f, 1.00f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNavFocus;

		int requestedActivation = -1;
		bool requestedBack = false;

		if (ImGui::Begin("##HFWTrainerMenu", nullptr, flags))
		{
			auto draw = ImGui::GetWindowDrawList();
			const auto windowPosition = ImGui::GetWindowPos();
			const auto font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize();

			const ImVec2 headerMin = windowPosition;
			const ImVec2 headerMax(windowPosition.x + width, windowPosition.y + headerHeight);
			draw->AddRectFilledMultiColor(headerMin, headerMax,
				IM_COL32(17, 124, 137, 255), IM_COL32(7, 60, 79, 255),
				IM_COL32(4, 39, 53, 255), IM_COL32(10, 83, 96, 255));
			draw->AddRectFilled(ImVec2(headerMin.x, headerMax.y - 4.0f * scale), headerMax, IM_COL32(236, 178, 66, 255));
			draw->AddText(font, fontSize * 1.28f, ImVec2(headerMin.x + 20.0f * scale, headerMin.y + 16.0f * scale),
				IM_COL32(255, 255, 255, 255), "HORIZON FORBIDDEN WEST");
			draw->AddText(font, fontSize * 0.95f, ImVec2(headerMin.x + 21.0f * scale, headerMin.y + 58.0f * scale),
				IM_COL32(238, 251, 252, 255), "游戏调整与修改器菜单");

			ImGui::SetCursorScreenPos(ImVec2(windowPosition.x, windowPosition.y + headerHeight));
			ImGui::Dummy(ImVec2(width, breadcrumbHeight));
			const ImVec2 breadcrumbMin(windowPosition.x, windowPosition.y + headerHeight);
			const ImVec2 breadcrumbMax(windowPosition.x + width, breadcrumbMin.y + breadcrumbHeight);
			draw->AddRectFilled(breadcrumbMin, breadcrumbMax, IM_COL32(7, 17, 23, 250));
			draw->AddText(font, fontSize, ImVec2(breadcrumbMin.x + 20.0f * scale, breadcrumbMin.y + (breadcrumbHeight - fontSize) * 0.5f),
				IM_COL32(218, 250, 252, 255), GetPageTitle());

			if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
			{
				if (io.MouseWheel > 0.0f)
					state.SelectedIndex = state.SelectedIndex == 0 ? Items.size() - 1 : state.SelectedIndex - 1;
				else
					state.SelectedIndex = (state.SelectedIndex + 1) % Items.size();

				if (state.SelectedIndex < state.ScrollOffset)
					state.ScrollOffset = state.SelectedIndex;
				else if (state.SelectedIndex >= state.ScrollOffset + visibleRows)
					state.ScrollOffset = state.SelectedIndex - visibleRows + 1;
			}

			for (size_t visibleIndex = 0; visibleIndex < visibleRows; visibleIndex++)
			{
				const size_t itemIndex = state.ScrollOffset + visibleIndex;
				auto& item = Items[itemIndex];
				const ImVec2 rowMin(windowPosition.x, breadcrumbMax.y + rowHeight * visibleIndex);
				const ImVec2 rowMax(windowPosition.x + width, rowMin.y + rowHeight);

				ImGui::SetCursorScreenPos(rowMin);
				ImGui::PushID(static_cast<int>(itemIndex));
				ImGui::InvisibleButton("##TrainerRow", ImVec2(width, rowHeight));
				const bool hovered = ImGui::IsItemHovered();
				if (hovered)
					state.SelectedIndex = itemIndex;

				const bool selected = state.SelectedIndex == itemIndex;
				if (selected)
				{
					draw->AddRectFilled(rowMin, rowMax, item.Enabled ? IM_COL32(223, 239, 239, 248) : IM_COL32(72, 82, 84, 245));
					draw->AddRectFilled(rowMin, ImVec2(rowMin.x + 4.0f * scale, rowMax.y), IM_COL32(226, 169, 60, 255));
				}
				else if (hovered)
					draw->AddRectFilled(rowMin, rowMax, IM_COL32(24, 71, 79, 220));

				draw->AddLine(ImVec2(rowMin.x, rowMax.y), rowMax, IM_COL32(43, 65, 70, 145));
				const auto textColor = !item.Enabled ? IM_COL32(151, 160, 162, 255)
					: selected ? IM_COL32(10, 20, 23, 255) : IM_COL32(255, 255, 255, 255);
				const auto valueColor = !item.Enabled ? IM_COL32(145, 154, 156, 255)
					: selected ? IM_COL32(15, 76, 83, 255) : IM_COL32(205, 250, 254, 255);
				const float rowTextY = rowMin.y + (rowHeight - fontSize) * 0.5f;
				draw->AddText(font, fontSize, ImVec2(rowMin.x + 20.0f * scale, rowTextY), textColor, item.Label.c_str());

				if (!item.Value.empty())
				{
					const auto valueWidth = ImGui::CalcTextSize(item.Value.c_str()).x;
					draw->AddText(font, fontSize, ImVec2(rowMax.x - valueWidth - 20.0f * scale, rowTextY),
						valueColor, item.Value.c_str());
				}

				if (item.Enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left))
					requestedActivation = static_cast<int>(itemIndex);
				ImGui::PopID();
			}

			const ImVec2 footerMin(windowPosition.x, breadcrumbMax.y + rowHeight * visibleRows);
			const ImVec2 footerMax(windowPosition.x + width, footerMin.y + footerHeight);
			draw->AddRectFilled(footerMin, footerMax, IM_COL32(5, 13, 18, 252));
			draw->AddLine(footerMin, ImVec2(footerMax.x, footerMin.y), IM_COL32(31, 133, 143, 220));

			const auto pageCounter = std::format("{} / {}", state.SelectedIndex + 1, Items.size());
			const auto counterWidth = ImGui::CalcTextSize(pageCounter.c_str()).x;
			draw->AddText(font, fontSize * 0.92f, ImVec2(footerMax.x - counterWidth - 16.0f * scale, footerMin.y + 8.0f * scale),
				IM_COL32(240, 184, 72, 255), pageCounter.c_str());
			draw->AddText(font, fontSize * 0.92f, ImVec2(footerMin.x + 16.0f * scale, footerMin.y + 8.0f * scale),
				IM_COL32(242, 250, 251, 255), "方向键 / WASD：选择");
			draw->AddText(font, fontSize * 0.92f, ImVec2(footerMin.x + 16.0f * scale, footerMin.y + 37.0f * scale),
				IM_COL32(242, 250, 251, 255), "回车：确认  退格：返回  INS：关闭");
			const auto& description = Items[state.SelectedIndex].Description;
			draw->AddText(font, fontSize, ImVec2(footerMin.x + 16.0f * scale, footerMin.y + 72.0f * scale),
				IM_COL32(255, 255, 255, 255), description.c_str(), nullptr, width - 32.0f * scale);

			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				requestedBack = true;
		}

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);

		if (requestedBack)
			GoBack();
		else if (requestedActivation >= 0 && std::cmp_less(requestedActivation, Items.size()))
			ActivateItem(Items[requestedActivation]);
	}

	void MainMenuBar::ActivateItem(MenuItem& Item)
	{
		if (Item.Enabled && Item.Activate)
			Item.Activate();
	}

	void MainMenuBar::OpenPage(Page Target)
	{
		m_Navigation.emplace_back(NavigationState { .PageId = Target });
	}

	void MainMenuBar::GoBack()
	{
		if (m_Navigation.size() > 1)
			m_Navigation.pop_back();
		else
			SetMenuVisible(false);
	}

	const char *MainMenuBar::GetPageTitle() const
	{
		switch (m_Navigation.back().PageId)
		{
		case Page::Home: return "主菜单";
		case Page::Player: return "玩家选项";
		case Page::Combat: return "战斗强化";
		case Page::Survival: return "生存与技能";
		case Page::Resources: return "资源与成长";
		case Page::World: return "世界与时间";
		case Page::Teleport: return "传送";
		case Page::Faction: return "玩家阵营";
		case Page::Utilities: return "实用工具";
		case Page::Developer: return "开发者工具";
		case Page::ConfirmExit: return "确认结束游戏";
		}

		return "主菜单";
	}

	void MainMenuBar::TeleportTo(const WorldPosition& Target)
	{
		auto position = Target;
		position.Z += 0.5;

		if (m_FreeCamMode == FreeCamMode::Noclip)
		{
			m_UndoPosition = m_FreeCamPosition.Position;
			m_FreeCamPosition.Position = position;
			return;
		}

		if (auto player = Player::GetLocalPlayer(); player && player->m_Entity)
			m_UndoPosition = player->m_Entity->GetWorldTransform().Position;

		JobHeaderCPU::SubmitCallable([position]()
		{
			auto player = Player::GetLocalPlayer();
			auto entity = player ? player->m_Entity : nullptr;
			if (!entity || !entity->m_Mover)
				return;

			auto worldTransform = entity->GetWorldTransform();
			worldTransform.Position = position;
			entity->m_Mover->OverrideMovement(worldTransform, 0.0001f, false);
		});
	}

	void MainMenuBar::DumpPlayerComponents()
	{
		auto player = Player::GetLocalPlayer();
		auto entity = player ? player->m_Entity : nullptr;
		if (!entity)
			return;

		spdlog::info("Player RTTI: '{}' UUID: '{}'", entity->GetRTTI()->GetSymbolName(), entity->m_UUID);
		if (auto resource = entity->m_EntityResource.GetUntypedPtr())
			spdlog::info("\tResource RTTI: '{}' UUID: '{}'", resource->GetRTTI()->GetSymbolName(), resource->m_UUID);

		spdlog::info("");
		for (size_t i = 0; i < entity->m_Components.m_Components.size(); i++)
		{
			const auto& component = entity->m_Components.m_Components[i];
			if (!component)
			{
				spdlog::warn("Component {} is null", i);
				continue;
			}

			spdlog::info(
				"Component {}: RTTI: '{}' UUID: '{}' (0x{:X})",
				i,
				component->GetRTTI()->GetSymbolName(),
				component->m_UUID,
				reinterpret_cast<uintptr_t>(component));

			if (auto resource = reinterpret_cast<RTTIRefObject *>(component->m_Resource.GetPtr()))
			{
				spdlog::info(
					"\tResource RTTI: '{}' UUID: '{}' (0x{:X})",
					resource->GetRTTI()->GetSymbolName(),
					resource->m_UUID,
					reinterpret_cast<uintptr_t>(resource));
			}
		}
	}

	void MainMenuBar::ToggleVisibility()
	{
		SetMenuVisible(!m_IsVisible);
	}

	void MainMenuBar::TogglePauseGameLogic()
	{
		m_PauseGame = !m_PauseGame;
	}

	void MainMenuBar::TogglePauseAIProcessing()
	{
		m_PauseAIProcessing = !m_PauseAIProcessing;
	}

	void MainMenuBar::TogglePauseTimeOfDay()
	{
		JobHeaderCPU::SubmitCallback([]()
		{
			if (auto gameModule = GameModule::GetInstance())
			{
				if (auto worldTimeState = gameModule->m_WorldTimeState)
					worldTimeState->m_IsPaused = !worldTimeState->m_IsPaused;
			}
		});
	}

	void MainMenuBar::ToggleQuickSave()
	{
		JobHeaderCPU::SubmitCallback([]()
		{
			auto player = static_cast<PlayerGame *>(Player::GetLocalPlayer());
			if (!player)
				return;

			const auto save = Offsets::Signature("40 57 48 83 EC 50 4C 8B 15 ? ? ? ? 4D 8B D9 41 0F B6 F8 4D 85 D2")
				.ToPointer<void(uint8_t, bool, bool, const GGUUID&)>();
			if (!save)
				return;

			player->m_RestartOnSpawned = true;
			save(2, false, false, {});
		});
	}

	void MainMenuBar::ToggleQuickLoad()
	{
		JobHeaderCPU::SubmitCallback([]()
		{
			if (!Player::GetLocalPlayer())
				return;

			const auto load = Offsets::Signature("40 55 57 48 8D 6C 24 B1 48 81 EC 88 00 00 00 48 8B 05")
				.ToPointer<void(float, uint8_t)>();
			if (load)
				load(0.0f, 1);
		});
	}

	void MainMenuBar::ToggleTimescaleOverride()
	{
		m_TimescaleOverride = !m_TimescaleOverride;
	}

	void MainMenuBar::AdjustTimescale(float Adjustment)
	{
		m_Timescale = std::clamp(m_Timescale + Adjustment, 0.001f, 10.0f);
		m_TimescaleOverride = true;
	}

	void MainMenuBar::ToggleFreeflyCamera()
	{
		auto player = Player::GetLocalPlayer();
		auto camera = player ? player->GetLastActivatedCamera() : nullptr;
		if (!camera)
			return;

		m_FreeCamMode = m_FreeCamMode == FreeCamMode::Free ? FreeCamMode::Off : FreeCamMode::Free;
		if (m_FreeCamMode == FreeCamMode::Free)
			m_FreeCamPosition = camera->GetWorldTransform();
	}

	void MainMenuBar::ToggleNoclip()
	{
		auto player = Player::GetLocalPlayer();
		auto entity = player ? player->m_Entity : nullptr;
		if (!entity)
			return;

		m_FreeCamMode = m_FreeCamMode == FreeCamMode::Noclip ? FreeCamMode::Off : FreeCamMode::Noclip;
		m_FreeCamPosition = entity->GetWorldTransform();
	}
}
