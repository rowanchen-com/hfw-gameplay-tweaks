#include "PreviewUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace Preview
{
    namespace
    {
        constexpr ImVec4 Background = ImVec4(0.030f, 0.033f, 0.045f, 0.985f);
        constexpr ImVec4 Sidebar = ImVec4(0.038f, 0.041f, 0.054f, 0.995f);
        constexpr ImVec4 Card = ImVec4(0.052f, 0.054f, 0.071f, 0.96f);
        constexpr ImVec4 CardHover = ImVec4(0.068f, 0.071f, 0.092f, 0.98f);
        constexpr ImVec4 Accent = ImVec4(0.29f, 0.48f, 1.00f, 1.00f);
        constexpr ImVec4 Text = ImVec4(0.93f, 0.94f, 0.98f, 1.00f);
        constexpr ImVec4 Muted = ImVec4(0.48f, 0.50f, 0.58f, 1.00f);

        enum class Page
        {
            Player,
            Movement,
            Resources,
            World,
            Teleport,
            Settings,
        };

        struct MockState
        {
            bool Invincible = false;
            bool InfiniteHealth = true;
            bool IgnoreDamage = false;
            bool InfiniteStamina = true;
            bool InstantBowCharge = false;
            bool InfiniteAmmo = false;
            bool InfiniteArrows = true;
            bool NoReload = false;
            bool FreeCamera = false;
            bool InfiniteJump = false;
            bool NoFallDamage = true;
            bool MountAnywhere = false;
            bool AiVersusAi = false;
            bool FreezeTime = false;
            bool OverrideWeather = false;
            bool FreeCrafting = false;
            bool RevealMap = false;
            bool ShowAdvanced = false;
            bool Chinese = true;
            float MovementSpeed = 3.0f;
            float JumpHeight = 5.0f;
            float FallingSpeed = 0.5f;
            float DamageMultiplier = 2.0f;
            float TimeOfDay = 12.0f;
            float UiScale = 1.0f;
            int Weather = 0;
            int ResourceAmount = 999;
        };

        MockState g_State;
        Page g_ActivePage = Page::Player;
        float g_PageFade = 1.0f;
        std::unordered_map<ImGuiID, float> g_ToggleAnimation;
        std::unordered_map<ImGuiID, float> g_HoverAnimation;

        float Animate(float current, float target, float speed = 14.0f)
        {
            const float delta = std::clamp(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f);
            return current + (target - current) * delta;
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return ImVec4(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t);
        }

        void SectionLabel(const char* text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Muted);
            ImGui::TextUnformatted(text);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        bool ToggleRow(const char* label, bool* value, const char* hint = nullptr)
        {
            const ImGuiID id = ImGui::GetID(label);
            const float rowHeight = ImGui::GetFrameHeight() + 12.0f;
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;

            ImGui::InvisibleButton(label, ImVec2(width, rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (clicked)
                *value = !*value;

            float& toggleT = g_ToggleAnimation[id];
            float& hoverT = g_HoverAnimation[id];
            toggleT = Animate(toggleT, *value ? 1.0f : 0.0f, 18.0f);
            hoverT = Animate(hoverT, hovered ? 1.0f : 0.0f, 18.0f);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            if (hoverT > 0.01f)
                draw->AddRectFilled(start, start + ImVec2(width, rowHeight), ImGui::GetColorU32(ImVec4(1, 1, 1, 0.028f * hoverT)), 7.0f);

            const float switchWidth = 45.0f;
            const float switchHeight = 24.0f;
            const ImVec2 switchMin(start.x + width - switchWidth - 4.0f, start.y + (rowHeight - switchHeight) * 0.5f);
            const ImVec2 switchMax = switchMin + ImVec2(switchWidth, switchHeight);
            const ImVec4 offColor = Mix(ImVec4(0.11f, 0.12f, 0.16f, 1.0f), ImVec4(0.15f, 0.16f, 0.21f, 1.0f), hoverT);
            draw->AddRectFilled(switchMin, switchMax, ImGui::GetColorU32(Mix(offColor, Accent, toggleT)), switchHeight * 0.5f);

            const float knobRadius = 9.0f;
            const float knobX = switchMin.x + 12.0f + toggleT * (switchWidth - 24.0f);
            draw->AddCircleFilled(ImVec2(knobX, switchMin.y + switchHeight * 0.5f), knobRadius,
                ImGui::GetColorU32(Mix(ImVec4(0.54f, 0.57f, 0.64f, 1.0f), ImVec4(1, 1, 1, 1), toggleT)), 24);

            const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
            draw->AddText(ImVec2(start.x + 5.0f, start.y + (rowHeight - textSize.y) * 0.5f), ImGui::GetColorU32(Text), label);

            if (hint && hovered)
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
                ImGui::TextUnformatted(hint);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            return clicked;
        }

        void SliderRow(const char* label, float* value, float minimum, float maximum, const char* format)
        {
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
            const float sliderWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.52f, 150.0f, 260.0f);
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - sliderWidth);
            ImGui::SetNextItemWidth(sliderWidth);
            ImGui::SliderFloat((std::string("##") + label).c_str(), value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        void IntegerInputRow(const char* label, int* value, int minimum, int maximum)
        {
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 150.0f);
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputInt((std::string("##") + label).c_str(), value, 1, 100);
            *value = std::clamp(*value, minimum, maximum);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        bool NavigationItem(const char* id, const char* symbol, const char* chinese, const char* english, Page page)
        {
            ImGui::PushID(id);
            const bool active = g_ActivePage == page;
            const float height = 49.0f;
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##nav", ImVec2(width, height));
            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked();

            const ImGuiID animationId = ImGui::GetID("##nav-animation");
            float& animation = g_HoverAnimation[animationId];
            animation = Animate(animation, (hovered || active) ? 1.0f : 0.0f, 15.0f);
            if (animation > 0.01f)
            {
                const ImVec4 background = active
                    ? Mix(ImVec4(0.10f, 0.11f, 0.15f, 0.65f), ImVec4(0.15f, 0.17f, 0.23f, 0.98f), animation)
                    : ImVec4(0.10f, 0.11f, 0.15f, 0.46f * animation);
                ImGui::GetWindowDrawList()->AddRectFilled(start, start + ImVec2(width, height), ImGui::GetColorU32(background), 8.0f);
            }
            if (active)
                ImGui::GetWindowDrawList()->AddRectFilled(start + ImVec2(0.0f, 11.0f), start + ImVec2(3.0f, height - 11.0f), ImGui::GetColorU32(Accent), 2.0f);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddText(start + ImVec2(15.0f, 14.0f), ImGui::GetColorU32(active ? Accent : Muted), symbol);
            const char* label = g_State.Chinese ? chinese : english;
            draw->AddText(start + ImVec2(48.0f, 14.0f), ImGui::GetColorU32(active ? Text : ImVec4(0.68f, 0.69f, 0.75f, 1.0f)), label);

            if (clicked && !active)
            {
                g_ActivePage = page;
                g_PageFade = 0.15f;
            }
            ImGui::PopID();
            return clicked;
        }

        bool BeginCard(const char* id, const char* title, float height)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Card);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 11.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            const bool visible = ImGui::BeginChild(id, ImVec2(0.0f, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushStyleColor(ImGuiCol_Text, Muted);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::Separator();
            return visible;
        }

        void EndCard()
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        void RenderPlayerPage()
        {
            const float availableHeight = ImGui::GetContentRegionAvail().y;
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float firstCardHeight = std::clamp(availableHeight * 0.55f, 300.0f, availableHeight - 210.0f);
            const float secondCardHeight = availableHeight - firstCardHeight - gap;
            if (ImGui::BeginTable("##player-columns", 2, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableNextColumn();
                if (BeginCard("##survival", g_State.Chinese ? "生存状态" : "SURVIVAL", firstCardHeight))
                {
                    ToggleRow(g_State.Chinese ? "无敌模式" : "Invincible", &g_State.Invincible, "模拟开关：不会访问游戏内存。");
                    ToggleRow(g_State.Chinese ? "无限生命" : "Infinite health", &g_State.InfiniteHealth);
                    ToggleRow(g_State.Chinese ? "无视伤害判定" : "Ignore damage checks", &g_State.IgnoreDamage);
                    ToggleRow(g_State.Chinese ? "无限体力" : "Infinite stamina", &g_State.InfiniteStamina);
                    ToggleRow(g_State.Chinese ? "免疫坠落伤害" : "No fall damage", &g_State.NoFallDamage);
                }
                EndCard();
                ImGui::Spacing();
                if (BeginCard("##actions", g_State.Chinese ? "动作与交互" : "ACTIONS", secondCardHeight))
                {
                    ToggleRow(g_State.Chinese ? "弓箭瞬间蓄力" : "Instant bow charge", &g_State.InstantBowCharge);
                    ToggleRow(g_State.Chinese ? "解除坐骑限制" : "Mount anywhere", &g_State.MountAnywhere);
                    ToggleRow(g_State.Chinese ? "允许 AI 互相伤害" : "Enable AI vs AI damage", &g_State.AiVersusAi);
                    ToggleRow(g_State.Chinese ? "显示高级选项" : "Show advanced options", &g_State.ShowAdvanced);
                }
                EndCard();

                ImGui::TableNextColumn();
                if (BeginCard("##weapons", g_State.Chinese ? "武器与弹药" : "WEAPONS & AMMO", firstCardHeight))
                {
                    ToggleRow(g_State.Chinese ? "无限弹药" : "Infinite ammo", &g_State.InfiniteAmmo);
                    ToggleRow(g_State.Chinese ? "无限箭矢与陷阱" : "Infinite arrows and traps", &g_State.InfiniteArrows);
                    ToggleRow(g_State.Chinese ? "无需装填" : "No reload", &g_State.NoReload);
                    SliderRow(g_State.Chinese ? "伤害倍率" : "Damage multiplier", &g_State.DamageMultiplier, 1.0f, 10.0f, "%.1fx");
                }
                EndCard();
                ImGui::Spacing();
                if (BeginCard("##status", g_State.Chinese ? "预览器状态" : "PREVIEW STATUS", secondCardHeight))
                {
                    ImGui::TextColored(Accent, g_State.Chinese ? "DLL 已动态加载" : "DLL loaded dynamically");
                    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.61f, 1.0f), g_State.Chinese ? "DirectX 12 渲染正常" : "DirectX 12 renderer ready");
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
                    ImGui::TextWrapped(g_State.Chinese
                        ? "这里的所有开关、数值和按钮都是 UI 模拟数据，不会读取地址，也不会修改任何游戏。"
                        : "All switches, values and buttons use mock data. No game addresses are read or changed.");
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                    ImGui::Text("FPS  %.0f", ImGui::GetIO().Framerate);
                }
                EndCard();
                ImGui::EndTable();
            }
        }

        void RenderMovementPage()
        {
            const float cardHeight = ImGui::GetContentRegionAvail().y;
            if (ImGui::BeginTable("##movement-columns", 2, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableNextColumn();
                if (BeginCard("##movement", g_State.Chinese ? "移动参数" : "MOVEMENT", cardHeight))
                {
                    SliderRow(g_State.Chinese ? "移动速度" : "Movement speed", &g_State.MovementSpeed, 1.0f, 8.0f, "%.1fx");
                    SliderRow(g_State.Chinese ? "跳跃高度" : "Jump height", &g_State.JumpHeight, 1.0f, 20.0f, "%.1f");
                    SliderRow(g_State.Chinese ? "下降速度" : "Falling speed", &g_State.FallingSpeed, 0.1f, 3.0f, "%.1fx");
                    ImGui::Spacing();
                    ToggleRow(g_State.Chinese ? "无限踏空跳" : "Infinite air jump", &g_State.InfiniteJump);
                    ToggleRow(g_State.Chinese ? "免疫坠落伤害" : "No fall damage", &g_State.NoFallDamage);
                }
                EndCard();
                ImGui::TableNextColumn();
                if (BeginCard("##camera", g_State.Chinese ? "镜头与穿墙" : "CAMERA & NOCLIP", cardHeight))
                {
                    ToggleRow(g_State.Chinese ? "自由飞行穿墙" : "Free-flight noclip", &g_State.FreeCamera);
                    SliderRow(g_State.Chinese ? "镜头移动速度" : "Camera speed", &g_State.MovementSpeed, 0.5f, 10.0f, "%.1fx");
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
                    ImGui::TextWrapped(g_State.Chinese
                        ? "快捷键预览：` / ~ 切换穿墙，WASD 移动，空格上升，Ctrl 下降。"
                        : "Hotkey preview: ` / ~ toggles noclip, WASD moves, Space rises and Ctrl descends.");
                    ImGui::PopStyleColor();
                }
                EndCard();
                ImGui::EndTable();
            }
        }

        void RenderResourcesPage()
        {
            const float cardHeight = ImGui::GetContentRegionAvail().y;
            if (ImGui::BeginTable("##resource-columns", 2, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableNextColumn();
                if (BeginCard("##resource-editor", g_State.Chinese ? "数量编辑" : "QUANTITY EDITOR", cardHeight))
                {
                    IntegerInputRow(g_State.Chinese ? "目标数量" : "Target amount", &g_State.ResourceAmount, 0, 999999);
                    ToggleRow(g_State.Chinese ? "无视制作与购买需求" : "Ignore crafting requirements", &g_State.FreeCrafting);
                    ImGui::Spacing();
                    if (ImGui::Button(g_State.Chinese ? "应用模拟数值" : "Apply mock value", ImVec2(-FLT_MIN, 42.0f)))
                        ImGui::OpenPopup("##applied");
                    if (ImGui::BeginPopup("##applied"))
                    {
                        ImGui::TextUnformatted(g_State.Chinese ? "模拟数值已应用（仅本窗口）。" : "Mock value applied to this window only.");
                        ImGui::EndPopup();
                    }
                }
                EndCard();
                ImGui::TableNextColumn();
                if (BeginCard("##inventory", g_State.Chinese ? "背包预览" : "INVENTORY PREVIEW", cardHeight))
                {
                    const std::array<const char*, 5> items = { "金属碎片", "药用浆果", "酸蚀猎手箭", "编织线", "资源袋" };
                    if (ImGui::BeginTable("##mock-items", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
                    {
                        ImGui::TableSetupColumn(g_State.Chinese ? "名称" : "Name", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn(g_State.Chinese ? "数量" : "Amount", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                        ImGui::TableHeadersRow();
                        for (size_t i = 0; i < items.size(); ++i)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(items[i]);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", static_cast<int>((i + 1) * 12));
                        }
                        ImGui::EndTable();
                    }
                }
                EndCard();
                ImGui::EndTable();
            }
        }

        void RenderWorldPage()
        {
            const float cardHeight = ImGui::GetContentRegionAvail().y;
            if (ImGui::BeginTable("##world-columns", 2, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableNextColumn();
                if (BeginCard("##time", g_State.Chinese ? "世界时间" : "WORLD TIME", cardHeight))
                {
                    ToggleRow(g_State.Chinese ? "锁定世界时间" : "Freeze world time", &g_State.FreezeTime);
                    SliderRow(g_State.Chinese ? "时间" : "Time of day", &g_State.TimeOfDay, 0.0f, 24.0f, "%.1f h");
                    ToggleRow(g_State.Chinese ? "显示完整地图" : "Reveal full map", &g_State.RevealMap);
                }
                EndCard();
                ImGui::TableNextColumn();
                if (BeginCard("##weather", g_State.Chinese ? "天气与环境" : "WEATHER & ENVIRONMENT", cardHeight))
                {
                    ToggleRow(g_State.Chinese ? "覆盖当前天气" : "Override current weather", &g_State.OverrideWeather);
                    const char* weatherItems[] = { "晴朗", "多云", "雨天", "沙尘暴" };
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::Combo("##weather-select", &g_State.Weather, weatherItems, IM_ARRAYSIZE(weatherItems));
                }
                EndCard();
                ImGui::EndTable();
            }
        }

        void RenderTeleportPage()
        {
            if (BeginCard("##teleport", g_State.Chinese ? "传送位置（模拟）" : "TELEPORT LOCATIONS (MOCK)", ImGui::GetContentRegionAvail().y))
            {
                const std::array<const char*, 6> locations = {
                    "基地", "炙矛地", "削链镇", "竞技场", "旧金山遗迹", "当前准星位置"
                };
                for (const char* location : locations)
                {
                    if (ImGui::Selectable(location, false, 0, ImVec2(0.0f, 42.0f)))
                        ImGui::OpenPopup("##teleport-note");
                }
                if (ImGui::BeginPopup("##teleport-note"))
                {
                    ImGui::TextUnformatted(g_State.Chinese ? "预览模式不会执行传送。" : "Preview mode does not teleport.");
                    ImGui::EndPopup();
                }
            }
            EndCard();
        }

        void RenderSettingsPage()
        {
            const float cardHeight = ImGui::GetContentRegionAvail().y;
            if (ImGui::BeginTable("##settings-columns", 2, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableNextColumn();
                if (BeginCard("##appearance", g_State.Chinese ? "界面" : "APPEARANCE", cardHeight))
                {
                    ToggleRow(g_State.Chinese ? "中文界面" : "Chinese interface", &g_State.Chinese);
                    SliderRow(g_State.Chinese ? "界面缩放预览" : "UI scale preview", &g_State.UiScale, 0.85f, 1.50f, "%.2fx");
                    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
                    ImGui::TextWrapped(g_State.Chinese
                        ? "正式版本会根据 1080p、2K、4K 和 Windows DPI 自动选择缩放。"
                        : "The production build will adapt to 1080p, 2K, 4K and Windows DPI.");
                    ImGui::PopStyleColor();
                }
                EndCard();
                ImGui::TableNextColumn();
                if (BeginCard("##keys", g_State.Chinese ? "快捷键" : "HOTKEYS", cardHeight))
                {
                    ImGui::TextUnformatted("INS");
                    ImGui::SameLine(150.0f);
                    ImGui::TextColored(Accent, g_State.Chinese ? "显示 / 隐藏菜单" : "Show / hide menu");
                    ImGui::Separator();
                    ImGui::TextUnformatted("ESC / X");
                    ImGui::SameLine(150.0f);
                    ImGui::TextColored(Accent, g_State.Chinese ? "关闭预览器" : "Close preview");
                    ImGui::Separator();
                    ImGui::TextUnformatted("` / ~");
                    ImGui::SameLine(150.0f);
                    ImGui::TextColored(Accent, g_State.Chinese ? "穿墙快捷键（仅展示）" : "Noclip hotkey (display only)");
                }
                EndCard();
                ImGui::EndTable();
            }
        }

        const char* PageTitle()
        {
            switch (g_ActivePage)
            {
            case Page::Player: return g_State.Chinese ? "玩家" : "Player";
            case Page::Movement: return g_State.Chinese ? "移动与镜头" : "Movement & Camera";
            case Page::Resources: return g_State.Chinese ? "资源与物品" : "Resources & Inventory";
            case Page::World: return g_State.Chinese ? "世界" : "World";
            case Page::Teleport: return g_State.Chinese ? "传送" : "Teleport";
            case Page::Settings: return g_State.Chinese ? "设置" : "Settings";
            default: return "HFW";
            }
        }
    }

    void ApplyStyle(float scale)
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(0.0f, 0.0f);
        style.FramePadding = ImVec2(12.0f, 8.0f);
        style.CellPadding = ImVec2(10.0f, 8.0f);
        style.ItemSpacing = ImVec2(12.0f, 10.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.ScrollbarSize = 13.0f;
        style.GrabMinSize = 12.0f;
        style.WindowRounding = 14.0f;
        style.ChildRounding = 10.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 9.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        style.TabRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ScaleAllSizes(std::clamp(scale, 1.0f, 1.5f));

        auto& colors = style.Colors;
        colors[ImGuiCol_Text] = Text;
        colors[ImGuiCol_TextDisabled] = Muted;
        colors[ImGuiCol_WindowBg] = Background;
        colors[ImGuiCol_ChildBg] = Card;
        colors[ImGuiCol_PopupBg] = ImVec4(0.055f, 0.058f, 0.076f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.10f, 0.11f, 0.15f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.075f, 0.078f, 0.100f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.11f, 0.145f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.13f, 0.16f, 0.25f, 1.0f);
        colors[ImGuiCol_TitleBg] = Sidebar;
        colors[ImGuiCol_TitleBgActive] = Sidebar;
        colors[ImGuiCol_CheckMark] = Accent;
        colors[ImGuiCol_SliderGrab] = Accent;
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.46f, 0.63f, 1.0f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.16f, 0.27f, 0.60f, 0.85f);
        colors[ImGuiCol_ButtonHovered] = Accent;
        colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.39f, 0.93f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.14f, 0.16f, 0.23f, 0.85f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.22f, 0.34f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.31f, 0.66f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.11f, 0.12f, 0.16f, 1.0f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.035f, 0.05f, 0.0f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.20f, 0.27f, 1.0f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.08f, 0.085f, 0.11f, 0.35f);
        colors[ImGuiCol_NavHighlight] = Accent;
    }

    void LoadFonts(float scale)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig fontConfig {};
        fontConfig.PixelSnapH = true;
        fontConfig.RasterizerMultiply = 1.15f;
        const float fontSize = 18.0f * std::clamp(scale, 1.0f, 1.5f);

        const std::vector<std::filesystem::path> candidates = {
            L"C:/Windows/Fonts/msyhbd.ttc",
            L"C:/Windows/Fonts/simhei.ttf",
            L"C:/Windows/Fonts/msyh.ttc",
        };

        for (const auto& path : candidates)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error))
                continue;
            const std::string utf8Path = path.string();
            if (ImFont* font = io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesChineseFull()))
            {
                io.FontDefault = font;
                return;
            }
        }
        fontConfig.SizePixels = fontSize;
        io.FontDefault = io.Fonts->AddFontDefault(&fontConfig);
    }

    void RenderMenu(bool& visible, bool& requestExit)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!visible)
        {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 24.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.80f);
            ImGui::Begin("##hidden-hint", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
            ImGui::TextUnformatted(g_State.Chinese ? "菜单已隐藏 · 按 INS 重新显示" : "Menu hidden · Press INS to show");
            ImGui::End();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            requestExit = true;

        const ImVec2 targetSize(
            std::clamp(io.DisplaySize.x * 0.78f, 980.0f, 1540.0f),
            std::clamp(io.DisplaySize.y * 0.82f, 660.0f, 940.0f));
        ImGui::SetNextWindowSize(targetSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Background);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin("HFW Menu Preview###MainPreviewWindow", nullptr, flags))
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const float sidebarWidth = 244.0f;
            const float topHeight = 72.0f;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(windowPos, windowPos + ImVec2(sidebarWidth, windowSize.y), ImGui::GetColorU32(Sidebar), 14.0f, ImDrawFlags_RoundCornersLeft);
            draw->AddLine(windowPos + ImVec2(sidebarWidth, 0.0f), windowPos + ImVec2(sidebarWidth, windowSize.y), ImGui::GetColorU32(ImVec4(0.10f, 0.11f, 0.15f, 1.0f)));
            draw->AddLine(windowPos + ImVec2(sidebarWidth, topHeight), windowPos + ImVec2(windowSize.x, topHeight), ImGui::GetColorU32(ImVec4(0.10f, 0.11f, 0.15f, 1.0f)));

            ImGui::SetCursorPos(ImVec2(22.0f, 20.0f));
            draw->AddRectFilled(windowPos + ImVec2(22.0f, 18.0f), windowPos + ImVec2(62.0f, 58.0f), ImGui::GetColorU32(ImVec4(0.055f, 0.10f, 0.20f, 1.0f)), 9.0f);
            draw->AddText(windowPos + ImVec2(31.0f, 28.0f), ImGui::GetColorU32(Accent), "HF");
            draw->AddText(windowPos + ImVec2(76.0f, 19.0f), ImGui::GetColorU32(Text), "HFW Lab");
            draw->AddText(windowPos + ImVec2(76.0f, 42.0f), ImGui::GetColorU32(Muted), "Menu Preview");

            ImGui::SetCursorPos(ImVec2(18.0f, 88.0f));
            ImGui::BeginChild("##navigation", ImVec2(sidebarWidth - 36.0f, windowSize.y - 160.0f), false, ImGuiWindowFlags_NoScrollbar);
            SectionLabel(g_State.Chinese ? "功能" : "FEATURES");
            NavigationItem("player", "P", "玩家", "Player", Page::Player);
            NavigationItem("movement", "M", "移动与镜头", "Movement", Page::Movement);
            NavigationItem("resources", "R", "资源与物品", "Resources", Page::Resources);
            NavigationItem("world", "W", "世界", "World", Page::World);
            NavigationItem("teleport", "T", "传送", "Teleport", Page::Teleport);
            ImGui::Spacing();
            SectionLabel(g_State.Chinese ? "通用" : "COMMON");
            NavigationItem("settings", "S", "设置", "Settings", Page::Settings);
            ImGui::EndChild();

            ImGui::SetCursorPos(ImVec2(22.0f, windowSize.y - 64.0f));
            ImGui::TextColored(Accent, "UI");
            ImGui::SameLine();
            ImGui::TextUnformatted(g_State.Chinese ? "仅界面预览" : "Visual preview only");

            ImGui::SetCursorPos(ImVec2(sidebarWidth + 30.0f, 22.0f));
            ImGui::TextUnformatted(PageTitle());
            ImGui::SameLine();
            ImGui::SetCursorPosX(sidebarWidth + 250.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, Muted);
            ImGui::TextUnformatted(g_State.Chinese ? "配置：预览 / 无游戏连接" : "Profile: preview / no game connection");
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(windowSize.x - 154.0f, 16.0f));
            if (ImGui::Button(g_State.Chinese ? "中 / EN" : "EN / 中", ImVec2(92.0f, 38.0f)))
                g_State.Chinese = !g_State.Chinese;
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(38.0f, 38.0f)))
                requestExit = true;

            g_PageFade = Animate(g_PageFade, 1.0f, 10.0f);
            ImGui::SetCursorPos(ImVec2(sidebarWidth + 24.0f, topHeight + 22.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(g_PageFade, 0.15f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
            ImGui::BeginChild("##page-content", ImVec2(windowSize.x - sidebarWidth - 48.0f, windowSize.y - topHeight - 44.0f), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            switch (g_ActivePage)
            {
            case Page::Player: RenderPlayerPage(); break;
            case Page::Movement: RenderMovementPage(); break;
            case Page::Resources: RenderResourcesPage(); break;
            case Page::World: RenderWorldPage(); break;
            case Page::Teleport: RenderTeleportPage(); break;
            case Page::Settings: RenderSettingsPage(); break;
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }
}
