#include <algorithm>
#include <format>
#include <shared_mutex>
#include <unordered_set>
#include "../../ModConfiguration.h"
#include "../../ModCoreEvents.h"
#include "../Core/Entity.h"
#include "../Core/Inventory.h"
#include "../Core/JobHeaderCPU.h"
#include "../Core/Player.h"
#include "PlayerInventoryWindow.h"
#include "TrainerToolWindow.h"

namespace HRZ2::DebugUI
{
	static StreamingRefBase g_TargetRef;

	void InventoryItemSpawnCallback::OnLoaded(RTTIRefObject *Object, void *Userdata)
	{
		if constexpr (false)
			spdlog::info("Received inventory item callback with root UUID {}", Object->m_UUID);

		auto targetUUID = static_cast<PlayerInventoryWindow *>(Userdata)->m_NextItemSpawnUUID;
		auto targetCount = static_cast<PlayerInventoryWindow *>(Userdata)->m_NextItemCount;

		std::shared_lock lock(ModCoreEvents::GetInstance().m_CachedDataMutex);
		TrySpawn(targetUUID, targetCount);
		static_cast<PlayerInventoryWindow *>(Userdata)->m_StreamerRequestPending.store(false);
	}

	void InventoryItemSpawnCallback::OnUnloaded(RTTIRefObject *Object, void *Userdata) {}

	bool InventoryItemSpawnCallback::TrySpawn(const GGUUID& ItemUUID, uint32_t ItemCount)
	{
		if (ItemCount <= 0)
			return true;

		for (const auto entry : ModCoreEvents::GetInstance().m_CachedInventoryItems)
		{
			if (entry->m_UUID != ItemUUID)
				continue;

			JobHeaderCPU::SubmitCallable(
				[ItemCount, item = Ref(static_cast<EntityResource *>(entry))]()
				{
					auto entity = Player::GetLocalPlayer() ? Player::GetLocalPlayer()->m_Entity : nullptr;

					if (!entity)
						return;

					std::lock_guard lock(entity->m_EntityAccessMutex);
					auto inventory = entity->m_Components.FindComponentByRTTI<Inventory>(RTTI::FindTypeByName("Inventory"));

					if (!inventory)
						return;

					// Try inventory first, then fallback to stash
					if (!inventory->AddItem(item, ItemCount, true))
					{
						const auto addOverflowItem = Offsets::Signature("44 88 4C 24 20 44 89 44 24 18 53 55 56 57 41 55 41 57 48 83 EC 68")
														 .ToPointer<bool(EntityComponent *, EntityResource *, int, bool)>();

						auto overflow = entity->m_Components.FindComponentByRTTI(RTTI::FindTypeByName("InventoryOverflowComponent"));
						if (addOverflowItem && overflow)
							addOverflowItem(overflow, item, ItemCount, true);
					}
				});

			return true;
		}

		return false;
	}

	void PlayerInventoryWindow::Render()
	{
		TrainerToolWindow window(GetId().c_str(), "玩家物品栏", &m_WindowOpen, ImVec2(880.0f, 780.0f));
		if (!window)
			return;

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "警告：");
		ImGui::SameLine();
		ImGui::TextWrapped("生成、添加或删除任务物品可能永久破坏游戏进度。使用此工具前请新建存档，风险自负。");

		ImGui::TextUnformatted("筛选（包含、-排除）");
		m_NameFilter.Draw("##InventoryFilter", -FLT_MIN);
		ImGui::Checkbox("仅显示玩家物品栏中的物品###ShowOnlyPlayerInventoryItems", &m_FilterItemsInPlayerInventory);
		ImGui::Checkbox("显示游戏本地化名称###ShowLocalizedNames", &m_ShowLocalizedItemNames);
		ImGui::Checkbox("显示内部资源 ID（高级）###ShowInventoryResourceIds", &m_ShowResourceIds);

		const ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
										   ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
										   ImGuiTableFlags_SizingFixedFit;
		const int columnCount = m_ShowResourceIds ? 3 : 2;

		if (ImGui::BeginTable("inventory_item_list", columnCount, tableFlags))
		{
			ImGui::TableSetupColumn("名称###Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("数量###Count", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.0f);
			if (m_ShowResourceIds)
				ImGui::TableSetupColumn("内部资源 ID###ResourceUUID", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 20.0f);
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();

			struct SortEntry
			{
				GGUUID UUID;
				bool HasUUID = false;
				std::string Name;
				Ref<InventoryItem> Hint;
				uint32_t Amount = 0;

				operator const char *() const
				{
					return Name.c_str();
				}

				bool operator<(const SortEntry& Other) const
				{
					if (auto result = strcmp(*this, Other); result != 0)
						return result < 0;

					return HasUUID && Other.HasUUID ? UUID < Other.UUID : false;
				}
			};

			// Combine both the player's inventory and the cached item list
			auto playerEntity = Player::GetLocalPlayer() ? Player::GetLocalPlayer()->m_Entity : nullptr;

			if (playerEntity)
			{
				std::vector<SortEntry> sortedItems;
				std::unordered_set<GGUUID> knownInventoryItems;

				{
					// Retain references while holding the game mutex, then release it before filtering,
					// sorting, and drawing thousands of cached rows.
					std::lock_guard lock(playerEntity->m_EntityAccessMutex);
					auto playerInventory = playerEntity->m_Components.FindComponentByRTTI<Inventory>(RTTI::FindTypeByName("Inventory"));

					if (playerInventory)
					{
						for (const auto& item : playerInventory->m_Items)
						{
							SortEntry entry;
							entry.Hint = item.Value;
							entry.Amount = item.Value->m_Amount;

							const auto displayName = item.Value->GetDisplayName();
							entry.Name.assign(displayName.data(), displayName.size());

							if (item.Value->m_EntityResource)
							{
								entry.UUID = item.Value->m_EntityResource->m_UUID;
								entry.HasUUID = true;

								if (!m_ShowLocalizedItemNames) // Delocalize it. Fast binary search.
								{
									const auto itr = std::lower_bound(
										ModConfiguration.CachedInventoryItems.begin(),
										ModConfiguration.CachedInventoryItems.end(),
										entry.UUID);

									if (itr != ModConfiguration.CachedInventoryItems.end() && itr->UUID == entry.UUID)
										entry.Name = itr->Name;
								}

								knownInventoryItems.emplace(entry.UUID);
							}

							sortedItems.emplace_back(std::move(entry));
						}
					}
				}

				if (!m_FilterItemsInPlayerInventory)
				{
					for (const auto& item : ModConfiguration.CachedInventoryItems)
					{
						if (!knownInventoryItems.contains(item.UUID))
							sortedItems.emplace_back(SortEntry {
								.UUID = item.UUID,
								.HasUUID = true,
								.Name = item.Name,
							});
					}
				}

				// Sort and filter
				std::erase_if(
					sortedItems,
					[&](const auto& Entry)
					{
						if (!m_NameFilter.IsActive())
							return false;

						std::string searchText = Entry.Name;
						if (Entry.HasUUID)
							searchText += std::format(" {}", Entry.UUID);

						return !m_NameFilter.PassFilter(searchText.c_str());
					});

				std::ranges::sort(sortedItems);

				// Only build ImGui rows that are visible in the scrolling table.
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(sortedItems.size()));

				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
						const auto& entry = sortedItems[i];
						const auto resourceUUID = entry.HasUUID ? entry.UUID : GGUUID {};

						ImGui::PushID(entry.HasUUID ? static_cast<const void *>(&entry.UUID) : static_cast<const void *>(entry.Hint.GetPtr()));
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Selectable(entry, false, ImGuiSelectableFlags_SpanAllColumns);
						DrawTableContextMenu(entry.Hint.GetPtr(), resourceUUID);

						if (entry.Hint)
						{
							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%u", entry.Amount);
						}

						if (m_ShowResourceIds)
						{
							ImGui::TableSetColumnIndex(2);
							const auto resourceIdText = std::format("{}", resourceUUID);
							ImGui::TextUnformatted(resourceIdText.c_str());
						}

						ImGui::PopID();
					}
				}
			}

			ImGui::EndTable();
		}

	}

	void PlayerInventoryWindow::DrawTableContextMenu(InventoryItem *Item, const GGUUID& ItemUUID)
	{
		if (!ImGui::BeginPopupContextItem("IVITListRowPopup", ImGuiPopupFlags_MouseButtonLeft))
			return;

		int64_t itemCount = 0;

		if (ImGui::Selectable("增加 1 个###AddOne", false, 0, ImVec2(260.0f * GetTrainerUIScale(), 0)))
			itemCount += 1;

		if (ImGui::Selectable("增加 5 个###AddFive"))
			itemCount += 5;

		// Only able to remove existing items
		if (Item)
		{
			if (ImGui::Selectable("增加当前数量的两倍###AddDouble"))
				itemCount += static_cast<int64_t>(Item->m_Amount) * 2;

			ImGui::Selectable("##sepsel1", false, ImGuiSelectableFlags_Disabled);

			if (ImGui::Selectable("移除 1 个###RemoveOne"))
				itemCount -= 1;

			if (ImGui::Selectable("移除 5 个###RemoveFive"))
				itemCount -= 5;

			if (ImGui::Selectable("移除一半###RemoveHalf"))
				itemCount -= std::max(Item->m_Amount / 2, 1u);

			ImGui::Selectable("##sepsel2", false, ImGuiSelectableFlags_Disabled);

			if (ImGui::Selectable("全部移除###RemoveAll"))
				itemCount -= Item->m_Amount;
		}

		if (itemCount < 0)
		{
			// Removal doesn't need special handling
			JobHeaderCPU::SubmitCallable([itemCount, item = Ref(Item)]
			{
				auto entity = Player::GetLocalPlayer() ? Player::GetLocalPlayer()->m_Entity : nullptr;

				if (!entity)
					return;

				std::lock_guard lock(entity->m_EntityAccessMutex);
				auto inventory = entity->m_Components.FindComponentByRTTI<Inventory>(RTTI::FindTypeByName("Inventory"));

				if (!inventory)
					return;

				inventory->RemoveItem(item, static_cast<uint32_t>(-itemCount), EInventoryItemRemoveType::Destroy, true);
			});
		}
		else if (itemCount != 0)
		{
			const auto targetItemCount = static_cast<uint32_t>(itemCount);

			const bool itemWasAlreadySpawned = [&]()
			{
				std::shared_lock lock(ModCoreEvents::GetInstance().m_CachedDataMutex);
				return m_LoaderCallback.TrySpawn(ItemUUID, targetItemCount);
			}();

			if (!itemWasAlreadySpawned)
			{
				if (m_StreamerRequestPending.exchange(true))
				{
					spdlog::warn("Ignored an inventory spawn request because another item is still streaming.");
				}
				else
				{
					m_NextItemSpawnUUID = ItemUUID;
					m_NextItemCount = targetItemCount;

					// We have to manually resolve a root UUID now
					const auto itr = std::lower_bound(
						ModConfiguration.CachedInventoryItems.begin(),
						ModConfiguration.CachedInventoryItems.end(),
						m_NextItemSpawnUUID);

					if (itr != ModConfiguration.CachedInventoryItems.end() && itr->UUID == m_NextItemSpawnUUID)
					{
						auto streamingManager = StreamingManager::GetInstance();
						if (streamingManager)
						{
							g_TargetRef.Clear();
							streamingManager->Register2(g_TargetRef, {}, itr->RootUUID);
							streamingManager->RegisterCallback(g_TargetRef, EStreamingRefCallbackMode::OnLoad, &m_LoaderCallback, this);
							streamingManager->Resolve(g_TargetRef, EStreamingRefPriority::Normal);
						}
						else
						{
							m_StreamerRequestPending.store(false);
						}
					}
					else
					{
						m_StreamerRequestPending.store(false);
					}
				}
			}
		}

		ImGui::EndPopup();
	}

	bool PlayerInventoryWindow::Close()
	{
		return !m_WindowOpen;
	}

	std::string PlayerInventoryWindow::GetId() const
	{
		return "玩家物品栏###PlayerInventoryWindow";
	}
}
