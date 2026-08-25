#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include "../../ModConfiguration.h"
#include "../../ModCoreEvents.h"
#include "../Core/Player.h"
#include "../Core/Mover.h"
#include "../Core/Destructibility.h"
#include "../Core/DebugSettings.h"
#include "../Core/JobHeaderCPU.h"
#include "../Nx/NxD3DImpl.h"
#include "../Nx/NxDXGIImpl.h"
#include "EntitySpawnerWindow.h"
#include "DebugUI.h"
#include "MainMenuBar.h"

// This has to be forward declared as ImGui avoids leaking Win32 types in header files
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace HRZ2::DebugUI
{
	std::unordered_map<std::string, std::shared_ptr<Window>> m_Windows;

	std::vector<ID3D12CommandAllocator *> CommandAllocators;
	ID3D12GraphicsCommandList *CommandList;

	ID3D12DescriptorHeap *SrvDescriptorHeap;
	ID3D12DescriptorHeap *RtvDescriptorHeap;
	ID3D12Fence *RenderFence;
	HANDLE RenderFenceEvent;
	std::vector<uint64_t> FrameFenceValues;
	uint64_t NextFenceValue = 1;
	UINT RtvDescriptorSize;

	WNDPROC OriginalWndProc;
	bool InterceptInput;
	HCURSOR PreviousCursor;
	HWND PreviousCaptureWindow;
	POINT PreviousCursorPosition;
	RECT PreviousCursorClip;
	bool PreviousCursorPositionWasValid;
	bool PreviousCursorClipWasActive;
	int CursorVisibilityAdjustments;

	LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	static ImFont *LoadChineseFont(ImGuiIO& IO)
	{
		std::vector<std::filesystem::path> fontCandidates;

		if (!ModConfiguration.DebugMenuFontPath.empty())
		{
			std::filesystem::path configuredPath(ModConfiguration.DebugMenuFontPath);
			fontCandidates.emplace_back(
				configuredPath.is_absolute() ? configuredPath : InternalModConfig::GetModRelativePath(ModConfiguration.DebugMenuFontPath));
		}

		fontCandidates.emplace_back("C:/Windows/Fonts/msyh.ttc");
		fontCandidates.emplace_back("C:/Windows/Fonts/simhei.ttf");
		fontCandidates.emplace_back("C:/Windows/Fonts/simsun.ttc");

		for (const auto& fontPath : fontCandidates)
		{
			std::error_code error;

			if (!std::filesystem::is_regular_file(fontPath, error))
				continue;

			ImFontConfig fontConfig {};
			fontConfig.FontNo = 0;

			const auto fontPathString = fontPath.string();
			if (auto font = IO.Fonts->AddFontFromFileTTF(
					fontPathString.c_str(),
					ModConfiguration.DebugMenuFontSize,
					&fontConfig,
					IO.Fonts->GetGlyphRangesChineseFull()))
			{
				return font;
			}
		}

		return IO.Fonts->AddFontDefault();
	}

	static bool CheckResult(HRESULT Result, const char *Operation)
	{
		if (SUCCEEDED(Result))
			return true;

		spdlog::error("{} failed with HRESULT 0x{:08X}.", Operation, static_cast<uint32_t>(Result));
		return false;
	}

	static RECT GetVirtualScreenRect()
	{
		return {
			.left = GetSystemMetrics(SM_XVIRTUALSCREEN),
			.top = GetSystemMetrics(SM_YVIRTUALSCREEN),
			.right = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
			.bottom = GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN),
		};
	}

	static HCURSOR GetMenuCursor()
	{
		static const auto cursor = LoadCursor(nullptr, IDC_ARROW);
		return cursor;
	}

	static void SetHardwareCursorActive(bool Active)
	{
		if (!ModConfiguration.DebugMenuUseHardwareCursor)
			return;

		if (Active)
		{
			PreviousCursor = GetCursor();
			PreviousCaptureWindow = GetCapture();
			PreviousCursorPositionWasValid = GetCursorPos(&PreviousCursorPosition);
			const auto virtualScreen = GetVirtualScreenRect();
			PreviousCursorClipWasActive = GetClipCursor(&PreviousCursorClip) &&
				!EqualRect(&PreviousCursorClip, &virtualScreen);

			ClipCursor(nullptr);
			ReleaseCapture();

			CursorVisibilityAdjustments = 0;
			int displayCount = 0;
			do
			{
				displayCount = ShowCursor(TRUE);
				CursorVisibilityAdjustments++;
			} while (displayCount < 0);

			SetCursor(GetMenuCursor());
		}
		else
		{
			for (int i = 0; i < CursorVisibilityAdjustments; i++)
				ShowCursor(FALSE);

			CursorVisibilityAdjustments = 0;

			if (PreviousCursorPositionWasValid)
				SetCursorPos(PreviousCursorPosition.x, PreviousCursorPosition.y);

			if (PreviousCursorClipWasActive)
				ClipCursor(&PreviousCursorClip);

			if (PreviousCaptureWindow)
				SetCapture(PreviousCaptureWindow);

			SetCursor(PreviousCursor);
			PreviousCursor = nullptr;
			PreviousCaptureWindow = nullptr;
			PreviousCursorPositionWasValid = false;
			PreviousCursorClipWasActive = false;
		}
	}

	bool Initialize(NxDXGIImpl *DXGIImpl)
	{
		// Steal the device and window handle from the swap chain
		ID3D12Device *device = nullptr;
		if (!CheckResult(DXGIImpl->m_DXGISwapChain->GetDevice(IID_PPV_ARGS(&device)), "IDXGISwapChain::GetDevice"))
			return false;
		const std::unique_ptr<ID3D12Device, void (*)(ID3D12Device *)> deviceReference(device, [](ID3D12Device *Device)
		{
			Device->Release();
		});

		HWND windowHandle = nullptr;
		if (!CheckResult(DXGIImpl->m_DXGISwapChain->GetHwnd(&windowHandle), "IDXGISwapChain::GetHwnd"))
			return false;

		// Grab back buffers and create d3d resources
		const D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 16,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		};

		if (!CheckResult(deviceReference->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&SrvDescriptorHeap)), "Create SRV descriptor heap"))
			return false;

		const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 8,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};

		if (!CheckResult(deviceReference->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&RtvDescriptorHeap)), "Create RTV descriptor heap"))
			return false;

		RtvDescriptorSize = deviceReference->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CommandAllocators.resize(DXGIImpl->m_NumBuffers);
		FrameFenceValues.resize(DXGIImpl->m_NumBuffers);

		if (CommandAllocators.empty() || CommandAllocators.size() > rtvHeapDesc.NumDescriptors)
		{
			spdlog::error("Unsupported DXGI back-buffer count: {}.", CommandAllocators.size());
			return false;
		}

		ID3D12Resource *tempBackBuffer = nullptr;
		if (!CheckResult(DXGIImpl->m_DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(&tempBackBuffer)), "IDXGISwapChain::GetBuffer"))
			return false;

		auto backBufferDesc = tempBackBuffer->GetDesc();
		tempBackBuffer->Release();

		for (uint32_t i = 0; i < CommandAllocators.size(); i++)
		{
			if (!CheckResult(
					deviceReference->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocators[i])),
					"Create D3D12 command allocator"))
				return false;
		}

		if (!CheckResult(
				deviceReference->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocators[0], nullptr, IID_PPV_ARGS(&CommandList)),
				"Create D3D12 command list"))
			return false;

		if (!CheckResult(deviceReference->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&RenderFence)), "Create D3D12 render fence"))
			return false;

		RenderFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!RenderFenceEvent)
		{
			spdlog::error("CreateEventW failed for the D3D12 render fence.");
			return false;
		}

		if (!CheckResult(CommandList->Close(), "Close initial D3D12 command list"))
			return false;

		// Initialize ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		auto& style = ImGui::GetStyle();
		style.FrameBorderSize = 1;
		style.WindowRounding = 3;
		style.ChildRounding = 2;
		style.FrameRounding = 2;
		style.PopupRounding = 2;
		style.ScrollbarRounding = 2;
		style.GrabRounding = 2;
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.045f, 0.060f, 0.96f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.025f, 0.045f, 0.060f, 0.98f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.12f, 0.55f, 0.62f, 0.60f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.11f, 0.14f, 0.96f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.08f, 0.32f, 0.38f, 0.96f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.43f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.08f, 0.35f, 0.42f, 0.90f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.10f, 0.47f, 0.55f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.12f, 0.58f, 0.66f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.08f, 0.33f, 0.39f, 0.90f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.48f, 0.56f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.60f, 0.68f, 1.00f);

		auto& io = ImGui::GetIO();
		io.FontDefault = LoadChineseFont(io);
		io.FontGlobalScale = ModConfiguration.DebugMenuFontScale;
		io.MouseDrawCursor = false;

		// Create D3D12 resources
		if (!ImGui_ImplWin32_Init(windowHandle))
		{
			spdlog::error("ImGui Win32 backend initialization failed.");
			return false;
		}

		if (!ImGui_ImplDX12_Init(
			deviceReference.get(),
			DXGIImpl->m_NumBuffers,
			backBufferDesc.Format,
			SrvDescriptorHeap,
			SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()))
		{
			spdlog::error("ImGui D3D12 backend initialization failed.");
			return false;
		}

		SetLastError(ERROR_SUCCESS);
		const auto previousWndProc = SetWindowLongPtrA(windowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc));
		if (previousWndProc == 0 && GetLastError() != ERROR_SUCCESS)
		{
			spdlog::error("Failed to install the debug-menu window procedure (Win32 error {}).", GetLastError());
			return false;
		}
		OriginalWndProc = reinterpret_cast<WNDPROC>(previousWndProc);
		DebugUI::AddWindow(std::make_shared<MainMenuBar>());
		return true;
	}

	void AddWindow(std::shared_ptr<Window> Handle)
	{
		auto id = Handle->GetId();
		auto existing = m_Windows.find(id);

		if (existing == m_Windows.end())
			m_Windows.emplace(id, Handle);
		else
			existing->second->Reopen();
	}

	void SetMenuVisible(bool Visible)
	{
		if (MainMenuBar::m_IsVisible == Visible && InterceptInput == Visible)
			return;

		MainMenuBar::m_IsVisible = Visible;
		InterceptInput = Visible;

		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = Visible && !ModConfiguration.DebugMenuUseHardwareCursor;
		SetHardwareCursorActive(Visible);
	}

	void RenderUI()
	{
		if (InterceptInput && ModConfiguration.DebugMenuUseHardwareCursor)
		{
			// Frame-generation inserts images that never execute this DLL. Let the Windows compositor
			// draw the pointer independently, and undo any cursor clipping restored by the game.
			ClipCursor(nullptr);
			SetCursor(GetMenuCursor());
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		UpdatePlayerSpecific();
		UpdateFreecam();

		// Clear window focus for every frame that isn't intercepting input
		if (!InterceptInput)
			ImGui::FocusWindow(nullptr);

		// A copy is required because Render() might create new instances and invalidate iterators
		auto currentWindows = m_Windows;

		for (auto& entry : currentWindows)
		{
			auto& window = entry.second;

			// Keep window instances alive after they are closed. Streaming callbacks registered by
			// inventory, weather, and entity windows retain their userdata pointer asynchronously.
			if (!window->Close())
				window->Render();
		}

		ImGui::Render();
	}

	void RenderUID3D(NxD3DImpl *D3DImpl, NxDXGIImpl *DXGIImpl)
	{
		const auto drawData = ImGui::GetDrawData();

		if (drawData->Valid && drawData->CmdListsCount > 0)
		{
			const auto bufferIndex = DXGIImpl->m_DXGISwapChain->GetCurrentBackBufferIndex();
			if (bufferIndex >= CommandAllocators.size())
			{
				spdlog::error("DXGI returned an invalid back-buffer index: {}.", bufferIndex);
				return;
			}

			const auto pendingFenceValue = FrameFenceValues[bufferIndex];
			if (pendingFenceValue != 0 && RenderFence->GetCompletedValue() < pendingFenceValue)
			{
				if (!CheckResult(RenderFence->SetEventOnCompletion(pendingFenceValue, RenderFenceEvent), "Wait for D3D12 render fence"))
					return;

				if (WaitForSingleObject(RenderFenceEvent, 5000) != WAIT_OBJECT_0)
				{
					spdlog::error("Timed out waiting for the D3D12 UI command allocator.");
					return;
				}
			}

			const auto buffer = D3DImpl->GetD3D12GameBackBuffer(bufferIndex);

			// Create a brand new RTV each frame. Tracking this in engine code is too difficult.
			auto rtvHandle = RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandle.ptr += static_cast<SIZE_T>(bufferIndex) * RtvDescriptorSize;
			D3DImpl->GetD3D12Device()->CreateRenderTargetView(buffer, nullptr, rtvHandle);

			// Reset command allocator, command list, then draw
			auto allocator = CommandAllocators[bufferIndex];
			if (!CheckResult(allocator->Reset(), "Reset D3D12 command allocator"))
				return;

			D3D12_RESOURCE_BARRIER barrier = {
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition = {
					.pResource = buffer,
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
					.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
				},
			};

			if (!CheckResult(CommandList->Reset(allocator, nullptr), "Reset D3D12 command list"))
				return;
			CommandList->ResourceBarrier(1, &barrier);

			CommandList->SetDescriptorHeaps(1, &SrvDescriptorHeap);
			CommandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
			ImGui_ImplDX12_RenderDrawData(drawData, CommandList);

			std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
			CommandList->ResourceBarrier(1, &barrier);
			if (!CheckResult(CommandList->Close(), "Close D3D12 UI command list"))
				return;

			auto queue = D3DImpl->GetD3D12CommandQueue(0);
			if (!queue)
			{
				spdlog::error("The D3D12 direct command queue is unavailable.");
				return;
			}

			queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&CommandList));

			const auto submittedFenceValue = NextFenceValue++;
			if (CheckResult(queue->Signal(RenderFence, submittedFenceValue), "Signal D3D12 render fence"))
				FrameFenceValues[bufferIndex] = submittedFenceValue;
		}
	}

	std::optional<LRESULT> HandleMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch (Msg)
		{
		case WM_KEYDOWN:
		{
			if (ModConfiguration.Hotkeys.ToggleDebugUI == VK_OEM_3 && wParam == VK_OEM_8)
				wParam = VK_OEM_3; // Workaround for UK keyboard layouts

			const bool isModHotkey =
				wParam == ModConfiguration.Hotkeys.ToggleDebugUI ||
				wParam == ModConfiguration.Hotkeys.TogglePauseGameLogic ||
				wParam == ModConfiguration.Hotkeys.ToggleAIProcessing ||
				wParam == ModConfiguration.Hotkeys.TogglePauseTimeOfDay ||
				wParam == ModConfiguration.Hotkeys.ToggleFreeCamera ||
				wParam == ModConfiguration.Hotkeys.ToggleNoclip ||
				wParam == ModConfiguration.Hotkeys.IncreaseTimescale ||
				wParam == ModConfiguration.Hotkeys.DecreaseTimescale ||
				wParam == ModConfiguration.Hotkeys.ToggleTimescale ||
				wParam == ModConfiguration.Hotkeys.SpawnEntity ||
				wParam == ModConfiguration.Hotkeys.QuickSave ||
				wParam == ModConfiguration.Hotkeys.QuickLoad;

			// A held key generates repeated WM_KEYDOWN messages. Consume repeats for mod hotkeys
			// so toggles cannot oscillate and one-shot actions cannot run more than once per press.
			if (isModHotkey && (lParam & (1LL << 30)) != 0)
				return 1;

			if (wParam == ModConfiguration.Hotkeys.ToggleDebugUI)
			{
				// Toggle input blocking and the trainer menu as one state transition.
				MainMenuBar::ToggleVisibility();

				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.TogglePauseGameLogic)
			{
				MainMenuBar::TogglePauseGameLogic();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.ToggleAIProcessing)
			{
				MainMenuBar::TogglePauseAIProcessing();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.TogglePauseTimeOfDay)
			{
				MainMenuBar::TogglePauseTimeOfDay();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.ToggleFreeCamera)
			{
				MainMenuBar::ToggleFreeflyCamera();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.ToggleNoclip)
			{
				MainMenuBar::ToggleNoclip();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.IncreaseTimescale)
			{
				MainMenuBar::AdjustTimescale(0.25f);
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.DecreaseTimescale)
			{
				MainMenuBar::AdjustTimescale(-0.25f);
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.ToggleTimescale)
			{
				MainMenuBar::ToggleTimescaleOverride();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.SpawnEntity)
			{
				EntitySpawnerWindow::ForceSpawnEntityClick();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.QuickSave)
			{
				MainMenuBar::ToggleQuickSave();
				return 1;
			}
			else if (wParam == ModConfiguration.Hotkeys.QuickLoad)
			{
				MainMenuBar::ToggleQuickLoad();
				return 1;
			}
		}
		break;
		}

		// Allow imgui to process it
		ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);

		if (InterceptInput && ModConfiguration.DebugMenuUseHardwareCursor && Msg == WM_SETCURSOR)
		{
			SetCursor(GetMenuCursor());
			return TRUE;
		}

		if (ShouldInterceptInput())
		{
			const static std::unordered_set<UINT> blockedMessages = {
				WM_MOUSEMOVE,	WM_MOUSELEAVE,	  WM_LBUTTONDOWN, WM_LBUTTONDBLCLK, WM_RBUTTONDOWN, WM_RBUTTONDBLCLK,
				WM_MBUTTONDOWN, WM_MBUTTONDBLCLK, WM_XBUTTONDOWN, WM_XBUTTONDBLCLK, WM_LBUTTONUP,	WM_RBUTTONUP,
				WM_MBUTTONUP,	WM_XBUTTONUP,	  WM_MOUSEWHEEL,  WM_MOUSEHWHEEL,	WM_KEYDOWN,		WM_KEYUP,
				WM_SYSKEYDOWN,	WM_SYSKEYUP,	  WM_CHAR,		  WM_INPUT,
			};

			if (blockedMessages.contains(Msg))
				return 0;
		}

		// Return nullopt to indicate the game should process the message instead
		return std::nullopt;
	}

	LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		auto handled = HandleMessage(hWnd, Msg, wParam, lParam);

		if (handled)
			return handled.value();

		return CallWindowProcA(OriginalWndProc, hWnd, Msg, wParam, lParam);
	}

	bool ShouldInterceptInput()
	{
		return InterceptInput || MainMenuBar::m_FreeCamMode == MainMenuBar::FreeCamMode::Free;
	}

	void UpdatePlayerSpecific()
	{
		auto debugSettings = DebugSettings::GetInstance();
		auto player = Player::GetLocalPlayer();

		if (!debugSettings || !player || !player->m_Entity)
			return;

		if (MainMenuBar::m_EnableInfiniteClipAmmo)
			debugSettings->m_InfiniteSizeClip = true;

		if (auto destructibility = player->m_Entity->m_Destructibility)
		{
			if (MainMenuBar::m_EnableGodMode)
				destructibility->m_Invulnerable = true;

			if (MainMenuBar::m_EnableDemigodMode)
				destructibility->m_DieAtZeroHealth = false;
		}

		if (MainMenuBar::m_EnableAutoNeutralFaction)
		{
			JobHeaderCPU::SubmitCallback([]
			{
				auto& modEvents = ModCoreEvents::GetInstance();
				std::shared_lock lock(modEvents.m_CachedDataMutex);

				for (auto faction : modEvents.m_CachedAIFactions)
				{
					if (faction->m_UUID == GGUUID::Parse("057D8132-C39A-D247-80B4-E35DF0F42098"))
					{
						if (auto player = Player::GetLocalPlayer(); player && player->m_Entity)
							player->m_Entity->SetFaction(reinterpret_cast<AIFaction *>(faction));

						break;
					}
				}
			});
		}

		if (MainMenuBar::m_LODRangeModifier != std::numeric_limits<float>::max())
		{
			// Emulate RepresentationManagerGame::SetLODDistanceMultiplier
			const auto representationManager = *Offsets::Signature("48 8B 05 ? ? ? ? 48 8B 88 10 1C 00 00 48 63 80 08 1C 00 00 48 8D 14 C1")
													.AsRipRelative(7)
													.ToPointer<uintptr_t>();

			if (representationManager)
			{
				auto& gameViewArray = *reinterpret_cast<Array<uintptr_t> *>(representationManager + 0x1C08); // TODO

				for (auto gameView : gameViewArray)
					*reinterpret_cast<float *>(gameView + 0x468) = MainMenuBar::m_LODRangeModifier; // TODO
			}
		}
	}

	void UpdateFreecam()
	{
		const auto& io = ImGui::GetIO();
		const auto cameraMode = MainMenuBar::m_FreeCamMode;

		if (cameraMode == MainMenuBar::FreeCamMode::Off || !Player::GetLocalPlayer())
			return;

		// Set up the camera's rotation matrix
		float adjustYaw = 0.0f;
		float adjustPitch = 0.0f;

		if (cameraMode == MainMenuBar::FreeCamMode::Free)
		{
			// Convert mouse X/Y to yaw/pitch angles
			static float currentCursorX = 0.0f;
			static float currentCursorY = 0.0f;
			static float targetCursorX = 0.0f;
			static float targetCursorY = 0.0f;

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
			{
				targetCursorX += io.MouseDelta.x * 0.5f;
				targetCursorY += io.MouseDelta.y * 0.5f;
			}

			// Exponential decay view angle smoothing (https://stackoverflow.com/a/10228863)
			const double springiness = 60.0;
			const float mult = static_cast<float>(1.0 - std::exp(std::log(0.5) * springiness * io.DeltaTime));

			currentCursorX += (targetCursorX - currentCursorX) * mult;
			currentCursorY += (targetCursorY - currentCursorY) * mult;

			float degreesX = std::fmodf(currentCursorX, 360.0f);
			float degreesY = std::fmodf(currentCursorY, 360.0f);

			if (degreesX < 0)
				degreesX += 360.0f;

			if (degreesY < 0)
				degreesY += 360.0f;

			// Degrees to radians
			adjustYaw = degreesX * (3.14159f / 180.0f);
			adjustPitch = degreesY * (3.14159f / 180.0f);
		}

		// Scale camera velocity based on delta time
		float baseSpeed = io.DeltaTime * 5.0f;

		if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
			baseSpeed *= 10.0f;
		else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
			baseSpeed /= 5.0f;

		// WSAD keys for movement
		float forwardSpeed = 0.0f;
		float strafeSpeed = 0.0f;

		if (ImGui::IsKeyDown(ImGuiKey_W))
			forwardSpeed += baseSpeed;

		if (ImGui::IsKeyDown(ImGuiKey_S))
			forwardSpeed -= baseSpeed;

		if (ImGui::IsKeyDown(ImGuiKey_A))
			strafeSpeed -= baseSpeed;

		if (ImGui::IsKeyDown(ImGuiKey_D))
			strafeSpeed += baseSpeed;

		JobHeaderCPU::SubmitCallable([forwardSpeed, strafeSpeed, adjustYaw, adjustPitch]
		{
			auto player = Player::GetLocalPlayer();
			auto camera = player ? player->GetLastActivatedCamera() : nullptr;

			if (!camera)
				return;

			auto cameraMode = MainMenuBar::m_FreeCamMode;
			auto& cameraTransform = MainMenuBar::m_FreeCamPosition;

			if (cameraMode == MainMenuBar::FreeCamMode::Free)
			{
				cameraTransform.Orientation = RotMatrix(adjustYaw, adjustPitch, 0.0f);
				cameraTransform.Position += cameraTransform.Orientation.Forward() * forwardSpeed;
				cameraTransform.Position += cameraTransform.Orientation.Right() * strafeSpeed;

				camera->SetWorldTransform(cameraTransform);
			}
			else if (cameraMode == MainMenuBar::FreeCamMode::Noclip && player->m_Entity && player->m_Entity->m_Mover)
			{
				cameraTransform.Orientation = camera->GetWorldTransform().Orientation;
				cameraTransform.Position += cameraTransform.Orientation.Forward() * forwardSpeed;
				cameraTransform.Position += cameraTransform.Orientation.Right() * strafeSpeed;

				player->m_Entity->m_Mover->OverrideMovement(cameraTransform, 0.0001f, false);
			}
		});
	}
}
