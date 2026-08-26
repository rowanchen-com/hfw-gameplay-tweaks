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
		ImGui::TextColored(ImVec4(0.82f, 0.98f, 1.0f, 1.0f), "单击物品所在行或数量，可直接输入目标总数量。");

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
						if (ImGui::Selectable(entry, false, ImGuiSelectableFlags_SpanAllColumns))
							RequestItemCountEditor(entry.Hint.GetPtr(), resourceUUID, entry.Name, entry.Amount);

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

		RenderItemCountEditor();
	}

	void PlayerInventoryWindow::RequestItemCountEditor(
		InventoryItem *Item,
		const GGUUID& ItemUUID,
		const std::string& ItemName,
		uint32_t CurrentCount)
	{
		m_ItemCountEditorItem = Item;
		m_ItemCountEditorUUID = ItemUUID;
		m_ItemCountEditorName = ItemName;
		m_ItemCountEditorValue = static_cast<int>(std::min(CurrentCount, static_cast<uint32_t>(999999)));
		m_OpenItemCountEditor = true;
		m_FocusItemCountEditor = true;
	}

	void PlayerInventoryWindow::RenderItemCountEditor()
	{
		constexpr const char *popupId = "修改物品数量###InventoryItemCountEditor";
		const float scale = GetTrainerUIScale();
		const auto displaySize = ImGui::GetIO().DisplaySize;
		const float width = std::min(520.0f * scale, displaySize.x - 36.0f * scale);

		if (m_OpenItemCountEditor)
		{
			ImGui::OpenPopup(popupId);
			m_OpenItemCountEditor = false;
		}

		ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
			ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Appearing);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f * scale, 18.0f * scale));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, 1.25f * scale));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.018f, 0.030f, 0.038f, 0.995f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.72f, 0.28f, 1.00f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::BeginPopupModal(popupId, nullptr, flags))
		{
			const uint32_t currentCount = m_ItemCountEditorItem ? m_ItemCountEditorItem->m_Amount : 0;
			ImGui::TextWrapped("物品：%s", m_ItemCountEditorName.c_str());
			ImGui::Text("当前数量：%u", currentCount);
			ImGui::Spacing();
			ImGui::TextUnformatted("输入目标总数量（0 - 999999）");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (m_FocusItemCountEditor)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusItemCountEditor = false;
			}
			ImGui::InputInt("##InventoryItemTargetCount", &m_ItemCountEditorValue, 1, 10);
			m_ItemCountEditorValue = std::clamp(m_ItemCountEditorValue, 0, 999999);

			ImGui::TextWrapped("输入 0 会把该物品全部移除；修改只执行一次，不会锁定数量。");
			ImGui::Spacing();

			const float buttonGap = 12.0f * scale;
			const float buttonWidth = (width - 40.0f * scale - buttonGap) * 0.5f;
			if (ImGui::Button("确认修改", ImVec2(buttonWidth, 50.0f * scale)))
			{
				const auto targetCount = static_cast<uint32_t>(m_ItemCountEditorValue);
				const auto countDelta = static_cast<int64_t>(targetCount) - static_cast<int64_t>(currentCount);
				ApplyItemCountChange(m_ItemCountEditorItem.GetPtr(), m_ItemCountEditorUUID, countDelta);
				m_ItemCountEditorItem = nullptr;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine(0.0f, buttonGap);
			if (ImGui::Button("取消", ImVec2(buttonWidth, 50.0f * scale)))
			{
				m_ItemCountEditorItem = nullptr;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	void PlayerInventoryWindow::ApplyItemCountChange(InventoryItem *Item, const GGUUID& ItemUUID, int64_t CountDelta)
	{
		if (CountDelta < 0 && Item)
		{
			// Removal doesn't need special handling
			JobHeaderCPU::SubmitCallable([CountDelta, item = Ref(Item)]
			{
				auto entity = Player::GetLocalPlayer() ? Player::GetLocalPlayer()->m_Entity : nullptr;

				if (!entity)
					return;

				std::lock_guard lock(entity->m_EntityAccessMutex);
				auto inventory = entity->m_Components.FindComponentByRTTI<Inventory>(RTTI::FindTypeByName("Inventory"));

				if (!inventory)
					return;

				inventory->RemoveItem(item, static_cast<uint32_t>(-CountDelta), EInventoryItemRemoveType::Destroy, true);
			});
		}
		else if (CountDelta > 0)
		{
			const auto targetItemCount = static_cast<uint32_t>(CountDelta);

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
