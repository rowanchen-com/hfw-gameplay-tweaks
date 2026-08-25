#include <format>
#include <shared_mutex>
#include "../../ModConfiguration.h"
#include "../../ModCoreEvents.h"
#include "../Core/GameModule.h"
#include "../Core/JobHeaderCPU.h"
#include "../Core/WeatherSystem.h"
#include "WeatherSetupWindow.h"
#include "TrainerToolWindow.h"

namespace HRZ2::DebugUI
{
	static StreamingRefBase g_TargetRef;

	void WeatherSetupLoaderCallback::OnLoaded(RTTIRefObject *Object, void *Userdata)
	{
		if constexpr (false)
			spdlog::info("Received weather setup callback with root UUID {}", Object->m_UUID);

		auto targetUUID = static_cast<WeatherSetupWindow *>(Userdata)->m_NextWeatherSetupUUID;

		std::shared_lock lock(ModCoreEvents::GetInstance().m_CachedDataMutex);
		TrySetOverride(targetUUID);
		static_cast<WeatherSetupWindow *>(Userdata)->m_StreamerRequestPending.store(false);
	}

	void WeatherSetupLoaderCallback::OnUnloaded(RTTIRefObject *Object, void *Userdata) {}

	bool WeatherSetupLoaderCallback::TrySetOverride(const GGUUID& SetupUUID)
	{
		for (const auto setup : ModCoreEvents::GetInstance().m_CachedWeatherSetups)
		{
			if (setup->m_UUID != SetupUUID)
				continue;

			JobHeaderCPU::SubmitCallable(
				[p = Ref(setup)]()
				{
					if (auto module = GameModule::GetInstance())
					{
						if (auto weatherSystem = module->m_WeatherSystem)
							weatherSystem->SetWeatherOverride(
								reinterpret_cast<WeatherSetup *>(p.GetPtr()),
								1.0f,
								EWeatherOverrideType::NODEGRAPH_WEATHER_OVERRIDE);
					}
				});

			return true;
		}

		return false;
	}

	void WeatherSetupWindow::Render()
	{
		TrainerToolWindow window(GetId().c_str(), "天气设置", &m_WindowOpen, ImVec2(700.0f, 700.0f));
		if (!window)
			return;

		// Draw weather setup list
		m_SpawnerNameFilter.Draw("筛选（包含,-排除）###WeatherSetupFilter");

		if (ImGui::BeginListBox("##WeatherSetupSelector", ImVec2(-FLT_MIN, -100)))
		{
			for (size_t i = 0; i < ModConfiguration.CachedWeatherSetups.size(); i++)
			{
				const auto& weatherSetup = ModConfiguration.CachedWeatherSetups[i];

				char fullName[256] = {};
				std::format_to_n(fullName, std::size(fullName) - 1, "{}, {}", weatherSetup.UUID, weatherSetup.Name);

				if (m_SpawnerNameFilter.PassFilter(fullName))
				{
					const bool isSelected = m_LastSelectedIndex == i;

					if (ImGui::Selectable(fullName, isSelected, ImGuiSelectableFlags_AllowDoubleClick))
					{
						m_LastSelectedIndex = i;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							m_DoSetOnNextFrame = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndListBox();
		}

		const bool setIsAllowed = m_LastSelectedIndex < ModConfiguration.CachedWeatherSetups.size() &&
			!m_StreamerRequestPending.load();
		ImGui::BeginDisabled(!setIsAllowed);

		if ((ImGui::Button("应用###Set") || m_DoSetOnNextFrame) && setIsAllowed)
		{
			const auto targetSetupUUID = ModConfiguration.CachedWeatherSetups[m_LastSelectedIndex].UUID;

			// Skip the RootUUID dance when the setup is already loaded
			const bool setupWasAlreadyLoaded = [&]()
			{
				std::shared_lock lock(ModCoreEvents::GetInstance().m_CachedDataMutex);
				return m_LoaderCallback.TrySetOverride(targetSetupUUID);
			}();

			if (!setupWasAlreadyLoaded)
			{
				if (m_StreamerRequestPending.exchange(true))
				{
					spdlog::warn("Ignored a weather request because another setup is still streaming.");
				}
				else
				{
					m_NextWeatherSetupUUID = targetSetupUUID;
					auto streamingManager = StreamingManager::GetInstance();
					if (streamingManager)
					{
						g_TargetRef.Clear();
						streamingManager->Register2(g_TargetRef, {}, ModConfiguration.CachedWeatherSetups[m_LastSelectedIndex].RootUUID);
						streamingManager->RegisterCallback(g_TargetRef, EStreamingRefCallbackMode::OnLoad, &m_LoaderCallback, this);
						streamingManager->Resolve(g_TargetRef, EStreamingRefPriority::Normal);
					}
					else
					{
						m_StreamerRequestPending.store(false);
					}
				}
			}
		}

		ImGui::Spacing();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("注意：部分名称缺失，可以在模组配置文件中手动添加。");
		ImGui::PopTextWrapPos();
		ImGui::EndDisabled();

		m_DoSetOnNextFrame = false;
	}

	bool WeatherSetupWindow::Close()
	{
		return !m_WindowOpen;
	}

	std::string WeatherSetupWindow::GetId() const
	{
		return "天气设置###WeatherSetupWindow";
	}
}
