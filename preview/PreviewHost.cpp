#include <Windows.h>
#include <filesystem>
#include <string>

namespace
{
    using RunPreviewFn = int(WINAPI *)(HINSTANCE);

    std::filesystem::path GetExecutableDirectory()
    {
        std::wstring path(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        path.resize(length);
        return std::filesystem::path(path).parent_path();
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    const auto dllPath = GetExecutableDirectory() / L"HFWMenuPreview.dll";
    const HMODULE previewModule = LoadLibraryW(dllPath.c_str());
    if (!previewModule)
    {
        const std::wstring message = L"无法加载 HFWMenuPreview.dll。\n\n请确认 EXE 与 DLL 位于同一目录。\nWindows 错误代码：" +
            std::to_wstring(GetLastError());
        MessageBoxW(nullptr, message.c_str(), L"HFW 菜单预览器", MB_OK | MB_ICONERROR);
        return 1;
    }

    const auto runPreview = reinterpret_cast<RunPreviewFn>(GetProcAddress(previewModule, "RunMenuPreview"));
    if (!runPreview)
    {
        MessageBoxW(nullptr, L"DLL 中缺少 RunMenuPreview 导出函数。", L"HFW 菜单预览器", MB_OK | MB_ICONERROR);
        FreeLibrary(previewModule);
        return 2;
    }

    const int result = runPreview(instance);
    FreeLibrary(previewModule);
    return result;
}
