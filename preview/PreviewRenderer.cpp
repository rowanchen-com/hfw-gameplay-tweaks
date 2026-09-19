#include "PreviewRenderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <wrl/client.h>

#include "PreviewUI.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace Preview
{
    namespace
    {
        using Microsoft::WRL::ComPtr;
        constexpr UINT FrameCount = 3;

        struct FrameContext
        {
            ComPtr<ID3D12CommandAllocator> Allocator;
            UINT64 FenceValue = 0;
        };

        HWND g_Window = nullptr;
        std::array<FrameContext, FrameCount> g_Frames;
        std::array<ComPtr<ID3D12Resource>, FrameCount> g_BackBuffers;
        ComPtr<ID3D12Device> g_Device;
        ComPtr<ID3D12DescriptorHeap> g_RtvHeap;
        ComPtr<ID3D12DescriptorHeap> g_SrvHeap;
        ComPtr<ID3D12CommandQueue> g_CommandQueue;
        ComPtr<ID3D12GraphicsCommandList> g_CommandList;
        ComPtr<IDXGISwapChain3> g_SwapChain;
        ComPtr<ID3D12Fence> g_Fence;
        HANDLE g_FenceEvent = nullptr;
        UINT64 g_LastSignaledFence = 0;
        UINT g_RtvDescriptorSize = 0;
        UINT g_PendingWidth = 0;
        UINT g_PendingHeight = 0;
        bool g_MenuVisible = true;
        bool g_RequestExit = false;

        void WaitForFence(UINT64 fenceValue)
        {
            if (fenceValue == 0 || g_Fence->GetCompletedValue() >= fenceValue)
                return;

            g_Fence->SetEventOnCompletion(fenceValue, g_FenceEvent);
            WaitForSingleObject(g_FenceEvent, INFINITE);
        }

        void WaitForLastSubmittedFrame()
        {
            for (const auto& frame : g_Frames)
                WaitForFence(frame.FenceValue);
        }

        void ReleaseRenderTargets()
        {
            for (auto& buffer : g_BackBuffers)
                buffer.Reset();
        }

        bool CreateRenderTargets()
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = g_RtvHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < FrameCount; ++i)
            {
                if (FAILED(g_SwapChain->GetBuffer(i, IID_PPV_ARGS(&g_BackBuffers[i]))))
                    return false;

                g_Device->CreateRenderTargetView(g_BackBuffers[i].Get(), nullptr, handle);
                handle.ptr += g_RtvDescriptorSize;
            }
            return true;
        }

        bool CreateDevice(HWND window)
        {
            ComPtr<IDXGIFactory4> factory;
            if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
                return false;

            if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_Device))))
                return false;

            const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                .NumDescriptors = FrameCount,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                .NodeMask = 0,
            };
            if (FAILED(g_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_RtvHeap))))
                return false;

            const D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = 1,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                .NodeMask = 0,
            };
            if (FAILED(g_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_SrvHeap))))
                return false;

            const D3D12_COMMAND_QUEUE_DESC queueDesc {
                .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                .Priority = 0,
                .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                .NodeMask = 0,
            };
            if (FAILED(g_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_CommandQueue))))
                return false;

            for (auto& frame : g_Frames)
            {
                if (FAILED(g_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.Allocator))))
                    return false;
            }

            if (FAILED(g_Device->CreateCommandList(
                    0,
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    g_Frames[0].Allocator.Get(),
                    nullptr,
                    IID_PPV_ARGS(&g_CommandList))))
                return false;
            g_CommandList->Close();

            if (FAILED(g_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_Fence))))
                return false;
            g_FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!g_FenceEvent)
                return false;

            DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
            swapChainDesc.BufferCount = FrameCount;
            swapChainDesc.Width = 0;
            swapChainDesc.Height = 0;
            swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
            swapChainDesc.Stereo = FALSE;

            ComPtr<IDXGISwapChain1> swapChain1;
            if (FAILED(factory->CreateSwapChainForHwnd(
                    g_CommandQueue.Get(), window, &swapChainDesc, nullptr, nullptr, &swapChain1)))
                return false;
            if (FAILED(swapChain1.As(&g_SwapChain)))
                return false;

            factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
            g_RtvDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            return CreateRenderTargets();
        }

        void CleanupDevice()
        {
            if (g_Device)
                WaitForLastSubmittedFrame();
            ReleaseRenderTargets();
            g_SwapChain.Reset();
            g_CommandList.Reset();
            g_CommandQueue.Reset();
            g_SrvHeap.Reset();
            g_RtvHeap.Reset();
            for (auto& frame : g_Frames)
            {
                frame.Allocator.Reset();
                frame.FenceValue = 0;
            }
            g_Fence.Reset();
            g_Device.Reset();
            if (g_FenceEvent)
            {
                CloseHandle(g_FenceEvent);
                g_FenceEvent = nullptr;
            }
        }

        void ResizeSwapChain(UINT width, UINT height)
        {
            if (!g_SwapChain || width == 0 || height == 0)
                return;

            WaitForLastSubmittedFrame();
            ReleaseRenderTargets();
            for (auto& frame : g_Frames)
                frame.FenceValue = 0;

            if (SUCCEEDED(g_SwapChain->ResizeBuffers(
                    FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)))
                CreateRenderTargets();
        }

        void RenderFrame(ImDrawData* drawData)
        {
            const UINT frameIndex = g_SwapChain->GetCurrentBackBufferIndex();
            auto& frame = g_Frames[frameIndex];
            WaitForFence(frame.FenceValue);

            frame.Allocator->Reset();
            g_CommandList->Reset(frame.Allocator.Get(), nullptr);

            D3D12_RESOURCE_BARRIER barrier {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_BackBuffers[frameIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_CommandList->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_RtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += static_cast<SIZE_T>(frameIndex) * g_RtvDescriptorSize;
            const float clearColor[4] = { 0.035f, 0.040f, 0.055f, 1.0f };
            g_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
            g_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
            ID3D12DescriptorHeap* heaps[] = { g_SrvHeap.Get() };
            g_CommandList->SetDescriptorHeaps(1, heaps);
            ImGui_ImplDX12_RenderDrawData(drawData, g_CommandList.Get());

            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            g_CommandList->ResourceBarrier(1, &barrier);
            g_CommandList->Close();

            ID3D12CommandList* commandLists[] = { g_CommandList.Get() };
            g_CommandQueue->ExecuteCommandLists(1, commandLists);
            g_SwapChain->Present(1, 0);

            const UINT64 fenceValue = ++g_LastSignaledFence;
            g_CommandQueue->Signal(g_Fence.Get(), fenceValue);
            frame.FenceValue = fenceValue;
        }

        LRESULT WINAPI WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
                return true;

            switch (message)
            {
            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED)
                {
                    g_PendingWidth = static_cast<UINT>(LOWORD(lParam));
                    g_PendingHeight = static_cast<UINT>(HIWORD(lParam));
                }
                return 0;
            case WM_SYSCOMMAND:
                if ((wParam & 0xFFF0) == SC_KEYMENU)
                    return 0;
                break;
            case WM_CLOSE:
                DestroyWindow(window);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                break;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }

    int Run(HINSTANCE hostInstance)
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW windowClass {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_CLASSDC,
            .lpfnWndProc = WindowProcedure,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = hostInstance,
            .hIcon = LoadIconW(nullptr, IDI_APPLICATION),
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .hbrBackground = nullptr,
            .lpszMenuName = nullptr,
            .lpszClassName = L"HFWMenuPreviewWindow",
            .hIconSm = LoadIconW(nullptr, IDI_APPLICATION),
        };
        RegisterClassExW(&windowClass);

        RECT rect { 0, 0, 1440, 900 };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        g_Window = CreateWindowW(
            windowClass.lpszClassName,
            L"HFW 菜单动画预览（仅界面，无游戏功能）",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            hostInstance,
            nullptr);

        if (!g_Window || !CreateDevice(g_Window))
        {
            MessageBoxW(nullptr, L"DirectX 12 初始化失败。请确认显卡驱动支持 DirectX 12。", L"HFW 菜单预览器", MB_OK | MB_ICONERROR);
            CleanupDevice();
            if (g_Window)
                DestroyWindow(g_Window);
            UnregisterClassW(windowClass.lpszClassName, hostInstance);
            return 3;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = "HFWMenuPreview.ini";

        const float dpiScale = std::clamp(static_cast<float>(GetDpiForWindow(g_Window)) / 96.0f, 1.0f, 1.75f);
        ApplyStyle(dpiScale);
        LoadFonts(dpiScale);

        ImGui_ImplWin32_Init(g_Window);
        ImGui_ImplDX12_Init(
            g_Device.Get(),
            FrameCount,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            g_SrvHeap.Get(),
            g_SrvHeap->GetCPUDescriptorHandleForHeapStart(),
            g_SrvHeap->GetGPUDescriptorHandleForHeapStart());

        ShowWindow(g_Window, SW_SHOWDEFAULT);
        UpdateWindow(g_Window);

        MSG message {};
        bool running = true;
        while (running)
        {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                if (message.message == WM_QUIT)
                    running = false;
            }
            if (!running)
                break;

            if (g_PendingWidth != 0 && g_PendingHeight != 0)
            {
                ResizeSwapChain(g_PendingWidth, g_PendingHeight);
                g_PendingWidth = 0;
                g_PendingHeight = 0;
            }

            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))
                g_MenuVisible = !g_MenuVisible;

            RenderMenu(g_MenuVisible, g_RequestExit);
            if (g_RequestExit)
                PostMessageW(g_Window, WM_CLOSE, 0, 0);

            ImGui::Render();
            RenderFrame(ImGui::GetDrawData());
        }

        WaitForLastSubmittedFrame();
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDevice();
        if (IsWindow(g_Window))
            DestroyWindow(g_Window);
        g_Window = nullptr;
        UnregisterClassW(windowClass.lpszClassName, hostInstance);
        return 0;
    }
}
