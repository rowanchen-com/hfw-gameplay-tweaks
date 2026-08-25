#pragma once

#include <atomic>
#include <imgui.h>
#include "../Core/IStreamingManager.h"
#include "DebugUIWindow.h"

namespace HRZ2
{
	class RTTIRefObject;
}

namespace HRZ2::DebugUI
{
	class WeatherSetupLoaderCallback : public IStreamingRefCallback
	{
	public:
		virtual ~WeatherSetupLoaderCallback() = default;
		virtual void OnLoaded(RTTIRefObject *Object, void *Userdata) override;
		virtual void OnUnloaded(RTTIRefObject *Object, void *Userdata) override;
		bool TrySetOverride(const GGUUID& SetupUUID);
	};

	class WeatherSetupWindow : public Window
	{
		friend class WeatherSetupLoaderCallback;

	private:
		bool m_WindowOpen = true;

		WeatherSetupLoaderCallback m_LoaderCallback;
		GGUUID m_NextWeatherSetupUUID = {};
		std::atomic_bool m_StreamerRequestPending = false;
		bool m_DoSetOnNextFrame = false;
		bool m_ShowResourceIds = false;

		static inline size_t m_LastSelectedIndex = std::numeric_limits<size_t>::max();
		static inline ImGuiTextFilter m_SpawnerNameFilter;

	public:
		virtual void Render() override;
		virtual bool Close() override;
		virtual void Reopen() override { m_WindowOpen = true; }
		virtual std::string GetId() const override;
	};
}
