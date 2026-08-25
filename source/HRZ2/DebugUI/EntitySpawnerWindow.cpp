#include <algorithm>
#include <format>
#include <mutex>
#include <shared_mutex>
#include "../../ModConfiguration.h"
#include "../../ModCoreEvents.h"
#include "../Core/Entity.h"
#include "../Core/JobHeaderCPU.h"
#include "../Core/Player.h"
#include "../PCore/UUID.h"
#include "EntitySpawnerWindow.h"
#include "TrainerToolWindow.h"

namespace HRZ2::DebugUI
{
	static StreamingRefBase g_TargetRef;

	void EntitySpawnerLoaderCallback::OnLoaded(RTTIRefObject *Object, void *Userdata)
	{
		if constexpr (false)
			spdlog::info("Received entity spawner callback with root UUID {}", Object->m_UUID);
	}

	void EntitySpawnerLoaderCallback::OnUnloaded(RTTIRefObject *Object, void *Userdata) {}

	void EntitySpawnerWindow::Render()
	{
		TrainerToolWindow window(GetId().c_str(), "实体生成器", &m_WindowOpen, ImVec2(880.0f, 780.0f));
		if (!window)
			return;

		static int spawnCount = 1;
		static int spawnLocationType = 0;
		static WorldPosition customSpawnPosition;
		static Ref<RTTIRefObject> customFaction;

		const bool allowSpawn = m_LastSelectedSetupIndex < ModConfiguration.CachedSpawnSetups.size() && m_OutstandingSpawnCount == 0;
		const auto availableSize = ImGui::GetContentRegionAvail();
		const float paneSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float paneWidth = (availableSize.x - paneSpacing) * 0.5f;
		const float listPaneWidth = paneWidth;
		const float settingsPaneWidth = paneWidth;
		const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::BeginChild("##EntityListPane", ImVec2(listPaneWidth, availableSize.y), false, paneFlags);
		ImGui::TextUnformatted("筛选（包含、-排除）");
		m_SpawnerNameFilter.Draw("##EntitySpawnerFilter", -FLT_MIN);
		ImGui::Checkbox("显示内部资源 ID（高级）###ShowSpawnerResourceIds", &m_ShowResourceIds);

		if (ImGui::BeginListBox("##SpawnSetupSelector", ImVec2(-FLT_MIN, ImGui::GetContentRegionAvail().y)))
		{
			for (size_t i = 0; i < ModConfiguration.CachedSpawnSetups.size(); i++)
			{
				const auto& spawnSetup = ModConfiguration.CachedSpawnSetups[i];

				char searchName[256] = {};
				std::format_to_n(searchName, std::size(searchName) - 1, "{} {}", spawnSetup.Name, spawnSetup.UUID);

				if (m_SpawnerNameFilter.PassFilter(searchName))
				{
					char displayName[256] = {};
					if (m_ShowResourceIds)
						std::format_to_n(displayName, std::size(displayName) - 1, "{}  [{}]", spawnSetup.Name, spawnSetup.UUID);
					else
						std::format_to_n(displayName, std::size(displayName) - 1, "{}", spawnSetup.Name);

					const bool isSelected = m_LastSelectedSetupIndex == i;
					ImGui::PushID(static_cast<int>(i));

					if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_AllowDoubleClick))
					{
						m_LastSelectedSetupIndex = i;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							ForceSpawnEntityClick();
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
			}

			ImGui::EndListBox();
		}
		ImGui::EndChild();

		ImGui::SameLine(0.0f, paneSpacing);
		ImGui::BeginChild("##EntitySettingsPane", ImVec2(settingsPaneWidth, availableSize.y), true, paneFlags);
		ImGui::TextColored(ImVec4(0.82f, 0.98f, 1.0f, 1.0f), "生成设置");
		ImGui::Separator();

		ImGui::BeginDisabled(!allowSpawn);
		const float columnWidth = (ImGui::GetContentRegionAvail().x - paneSpacing) * 0.5f;

		ImGui::BeginGroup();
		ImGui::TextUnformatted("生成数量");
		ImGui::SetNextItemWidth(columnWidth);
		ImGui::InputInt("##entitycount", &spawnCount);
		spawnCount = std::max(spawnCount, 1);
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::TextUnformatted("玩家阵营");
		ImGui::SetNextItemWidth(columnWidth);
		{
			// Draw faction list
			auto& modEvents = ModCoreEvents::GetInstance();
			std::shared_lock lock(modEvents.m_CachedDataMutex);

			String previewString = "<未指定阵营>";
			
			if (!modEvents.m_CachedAIFactions.contains(customFaction.GetPtr()))
				customFaction = nullptr;
			else
				previewString = customFaction->GetMemberRefUnsafe<String>("Name");

			if (ImGui::BeginCombo("##factioncombo", previewString.c_str()))
			{
				std::vector sortedFactions(modEvents.m_CachedAIFactions.begin(), modEvents.m_CachedAIFactions.end());

				std::ranges::sort(
					sortedFactions,
					[](auto A, auto B)
					{
						return A->GetMemberRefUnsafe<String>("Name") < B->GetMemberRefUnsafe<String>("Name");
					});

				if (ImGui::Selectable("<未指定阵营>###UnsetFaction", !customFaction))
					customFaction = nullptr;

				for (auto faction : sortedFactions)
				{
					const bool isSelected = customFaction.GetPtr() == faction;

					if (ImGui::Selectable(faction->GetMemberRefUnsafe<String>("Name").c_str(), isSelected))
						customFaction = faction;

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
		}
		ImGui::EndGroup();
		ImGui::Spacing();
		ImGui::TextUnformatted("生成位置");
		ImGui::RadioButton("玩家###SpawnAtPlayerPosition", &spawnLocationType, 0);
		ImGui::SameLine();
		ImGui::RadioButton("准星###SpawnAtCrosshairPosition", &spawnLocationType, 1);
		ImGui::RadioButton("自定义###SpawnAtCustomPosition", &spawnLocationType, 2);
		ImGui::Spacing();

		if (spawnLocationType == 2)
		{
			ImGui::TextUnformatted("自定义坐标");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 1.5f);
			ImGui::InputDouble("X", &customSpawnPosition.X, 1.0, 20.0, "%.3f");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 1.5f);
			ImGui::InputDouble("Y", &customSpawnPosition.Y, 1.0, 20.0, "%.3f");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 1.5f);
			ImGui::InputDouble("Z", &customSpawnPosition.Z, 1.0, 20.0, "%.3f");
			ImGui::Spacing();
		}

		if (ImGui::Button("生成###Spawn", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)) || (m_DoSpawnOnNextFrame && allowSpawn))
		{
			if (auto transform = GetSpawnTransform(spawnLocationType, customSpawnPosition))
			{
				m_NextSpawnTransform = *transform;
				m_NextSpawnSelectedIndex = m_LastSelectedSetupIndex;
				m_NextFaction = customFaction;
				m_OutstandingSpawnCount = static_cast<uint32_t>(spawnCount);
			}
			else
			{
				spdlog::warn("Entity spawn was ignored because the player or camera was unavailable.");
			}
		}

		ImGui::EndDisabled();
		const float warningBlockHeight = ImGui::GetTextLineHeightWithSpacing() * 5.0f;
		const float warningStartY = ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y - warningBlockHeight;
		ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), warningStartY));
		ImGui::Separator();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.38f, 1.0f), "警告：人形和脚本实体可能导致游戏崩溃。");
		ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.38f, 1.0f), "部分名称缺失，可在模组配置文件中补充。");
		ImGui::PopTextWrapPos();
		ImGui::EndChild();

		RunSpawnCommands();
		m_DoSpawnOnNextFrame = false;
	}

	bool EntitySpawnerWindow::Close()
	{
		return !m_WindowOpen;
	}

	std::string EntitySpawnerWindow::GetId() const
	{
		return "实体生成器###EntitySpawnerWindow";
	}

	void EntitySpawnerWindow::RunSpawnCommands()
	{
		// Faction setup logic
		if (!m_FactionUpdateJobPending.exchange(true))
		{
			bool hasPendingEntries = false;
			{
				std::shared_lock lock(m_FactionSetupMutex);
				hasPendingEntries = !m_FactionSetsPending.empty();
			}

			if (hasPendingEntries)
			{
				JobHeaderCPU::SubmitCallable([this]
				{
					std::scoped_lock lock(m_FactionSetupMutex);

					std::erase_if(
						m_FactionSetsPending,
						[&](const auto& Pair)
						{
							const auto getSpawnpointEntity = Offsets::Signature(
																 "40 55 48 8D 6C 24 A9 48 81 EC A0 00 00 00 48 83 ? ? ? ? ? ? 48 8B C1 75 0B 33 C0")
																 .ToPointer<Entity *(RTTIRefObject *)>();

							if (auto entity = getSpawnpointEntity(Pair.first))
							{
								entity->SetFaction(reinterpret_cast<HRZ2::AIFaction *>(Pair.second.GetPtr()));
								return true;
							}

							return false;
						});

					m_FactionUpdateJobPending.store(false);
				});
			}
			else
			{
				m_FactionUpdateJobPending.store(false);
			}
		}

		// Streaming and spawnpoint logic
		if (m_OutstandingSpawnCount <= 0)
			return;

		const auto rootUUID = ModConfiguration.CachedSpawnSetups[m_NextSpawnSelectedIndex].RootUUID;
		const auto spawnSetupUUID = ModConfiguration.CachedSpawnSetups[m_NextSpawnSelectedIndex].UUID;

		const auto targetSpawnSetup = [&]() -> Ref<RTTIRefObject>
		{
			auto& modEvents = ModCoreEvents::GetInstance();
			std::shared_lock lock(modEvents.m_CachedDataMutex);

			auto itr = std::ranges::find_if(
				modEvents.m_CachedSpawnSetups,
				[&](const auto& Setup)
				{
					return Setup->m_UUID == spawnSetupUUID;
				});

			if (itr != modEvents.m_CachedSpawnSetups.end())
				return *itr;

			return nullptr;
		}();

		// If the setup isn't already loaded we'll have to stream the whole object group in
		if (!targetSpawnSetup && !m_StreamerRequestPending)
		{
			auto streamingManager = StreamingManager::GetInstance();

			g_TargetRef.Clear();
			streamingManager->Register2(g_TargetRef, {}, rootUUID);
			streamingManager->RegisterCallback(g_TargetRef, EStreamingRefCallbackMode::OnLoad, &m_LoaderCallback, this);
			streamingManager->Resolve(g_TargetRef, EStreamingRefPriority::Normal);

			m_StreamerRequestPending = true;
		}
		else if (targetSpawnSetup)
		{
			spdlog::debug(
				"Spawning {} entities with UUID {}",
				m_OutstandingSpawnCount,
				ModConfiguration.CachedSpawnSetups[m_NextSpawnSelectedIndex].UUID);

			JobHeaderCPU::SubmitCallable(
				[this,
				 spawnCount = m_OutstandingSpawnCount,
				 spawnSetup = targetSpawnSetup,
				 transform = m_NextSpawnTransform,
					 faction = m_NextFaction]()
				{
					const auto spawnpointRTTI = RTTI::FindTypeByName("Spawnpoint")->AsCompound();

					for (uint32_t i = 0; i < spawnCount; i++)
					{
						Ref spawnpoint = static_cast<RTTIRefObject *>(spawnpointRTTI->CreateInstance()); // TODO: MsgInit?

						spawnpointRTTI->SetMemberValue<GGUUID>(spawnpoint, "ObjectUUID", GGUUID::Generate());
						spawnpointRTTI->SetMemberValue<WorldTransform>(spawnpoint, "Orientation", transform);
						spawnpointRTTI->SetMemberValue<bool>(spawnpoint, "FactsArePersistent", false);
						spawnpointRTTI->SetMemberValue<bool>(spawnpoint, "AutoSpawn", false);
						spawnpointRTTI->SetMemberValue<Ref<RTTIRefObject>>(spawnpoint, "SpawnSetup", spawnSetup);

						const auto spawnpointSpawn = Offsets::Signature("48 85 C9 74 43 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 84 C0")
														 .ToPointer<void(RTTIRefObject *)>();
						spawnpointSpawn(spawnpoint);

						if (faction)
						{
							std::scoped_lock lock(m_FactionSetupMutex);
							m_FactionSetsPending.emplace_back(std::move(spawnpoint), faction);
						}
					}
				});

			m_OutstandingSpawnCount = 0;
			m_NextFaction = nullptr;
			m_StreamerRequestPending = false;
		}
	}

	std::optional<WorldTransform> EntitySpawnerWindow::GetSpawnTransform(uint32_t Type, const WorldPosition& CustomPosition)
	{
		auto player = Player::GetLocalPlayer();
		if (!player || !player->m_Entity)
			return std::nullopt;

		auto currentTransform = player->m_Entity->GetWorldTransform();

		if (Type == 0)
		{
			// Player position
			currentTransform.Position = player->m_Entity->m_WorldTransform.Position;
		}
		else if (Type == 1)
		{
			// Crosshair position - project forwards
			auto camera = player->GetLastActivatedCamera();
			if (!camera)
				return std::nullopt;

			const auto cameraMatrix = camera->GetWorldTransform();
			const auto moveDirection = cameraMatrix.Orientation.Forward() * 200.0f;

			currentTransform.Position += moveDirection;

			// Raycast
			WorldPosition rayHitPosition;
			float unknownFloat;
			Entity *unknownEntity;
			void *unknownVoid;
			Vec3 normal;
			uint32_t uint1;
			uint32_t uint2;

			const auto intersectLine = Offsets::Signature("4C 8B DC 49 89 5B 10 49 89 73 18 55 57 41 54 41 55 41 57 48 8D 6C 24 90")
										   .ToPointer<bool(
											   const WorldPosition&, // a1
											   const WorldPosition&, // a2
											   int,					 // a3 EPhysicsCollisionLayerGame
											   const Entity *,		 // a4
											   bool,				 // a5
											   uint8_t,				 // a6
											   int,					 // a7
											   WorldPosition *,		 // a8
											   Vec3 *,				 // a9
											   float *,				 // a10
											   Entity **,			 // a11
											   void **,				 // a12
											   uint32_t&,			 // a13
											   uint32_t&)>();		 // a14

			const bool didHit = intersectLine(
				cameraMatrix.Position,
				currentTransform.Position,
				47,
				nullptr,
				false,
				0,
				0,
				&rayHitPosition,
				&normal,
				&unknownFloat,
				&unknownEntity,
				&unknownVoid,
				uint1,
				uint2);

			if (didHit)
				currentTransform.Position = rayHitPosition;
		}
		else if (Type == 2)
		{
			// Custom position
			currentTransform.Position = CustomPosition;
		}

		return currentTransform;
	}

	void EntitySpawnerWindow::ForceSpawnEntityClick()
	{
		m_DoSpawnOnNextFrame = true;
	}
}
