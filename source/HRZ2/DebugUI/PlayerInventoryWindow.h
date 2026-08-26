#pragma once

#include <atomic>
#include <string>
#include <imgui.h>
#include "../Core/IStreamingManager.h"
#include "../Core/Inventory.h"
#include "DebugUIWindow.h"

namespace HRZ2
{
	class RTTIRefObject;
}

namespace HRZ2::DebugUI
{
	class InventoryItemSpawnCallback : public IStreamingRefCallback
	{
	public:
		virtual ~InventoryItemSpawnCallback() = default;
		virtual void OnLoaded(RTTIRefObject *Object, void *Userdata) override;
		virtual void OnUnloaded(RTTIRefObject *Object, void *Userdata) override;
		bool TrySpawn(const GGUUID& ItemUUID, uint32_t ItemCount);
	};

	class PlayerInventoryWindow : public Window
	{
		friend class InventoryItemSpawnCallback;

	private:
		bool m_WindowOpen = true;

		InventoryItemSpawnCallback m_LoaderCallback;
		GGUUID m_NextItemSpawnUUID;
		uint32_t m_NextItemCount = 0;
		std::atomic_bool m_StreamerRequestPending = false;

		ImGuiTextFilter m_NameFilter;
		bool m_FilterItemsInPlayerInventory = false;
		bool m_ShowLocalizedItemNames = true;
		bool m_ShowResourceIds = false;

		Ref<InventoryItem> m_ItemCountEditorItem;
		GGUUID m_ItemCountEditorUUID;
		std::string m_ItemCountEditorName;
		int m_ItemCountEditorValue = 0;
		bool m_OpenItemCountEditor = false;
		bool m_FocusItemCountEditor = false;

	public:
		virtual void Render() override;
		virtual bool Close() override;
		virtual void Reopen() override { m_WindowOpen = true; }
		virtual std::string GetId() const override;

	private:
		void RequestItemCountEditor(InventoryItem *Item, const GGUUID& ItemUUID, const std::string& ItemName, uint32_t CurrentCount);
		void RenderItemCountEditor();
		void ApplyItemCountChange(InventoryItem *Item, const GGUUID& ItemUUID, int64_t CountDelta);
	};
}
