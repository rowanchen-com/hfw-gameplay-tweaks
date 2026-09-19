#include "PreviewUI.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace Preview
{
    namespace
    {
        namespace Theme
        {
            constexpr ImVec4 Accent { 0.30f, 0.49f, 1.00f, 1.00f };
            constexpr ImVec4 Text { 0.96f, 0.97f, 1.00f, 1.00f };
            constexpr ImVec4 Muted { 0.51f, 0.52f, 0.56f, 1.00f };
            constexpr ImVec4 Border { 1.00f, 1.00f, 1.00f, 0.035f };
            constexpr ImVec4 Window { 0.015f, 0.021f, 0.036f, 0.985f };
            constexpr ImVec4 Sidebar { 0.012f, 0.018f, 0.031f, 0.995f };
            constexpr ImVec4 FrameOff { 0.023f, 0.039f, 0.070f, 1.00f };
            constexpr ImVec4 FrameOn { 0.043f, 0.070f, 0.137f, 1.00f };
            constexpr ImVec4 Button { 0.031f, 0.035f, 0.058f, 1.00f };
            constexpr ImVec4 ButtonHover { 0.050f, 0.054f, 0.078f, 1.00f };
            constexpr ImVec4 Group { 0.019f, 0.035f, 0.062f, 1.00f };
        }

        enum class Page { Player, Resources, World, Developer, Settings };
        enum class Icon { User, Move, Bag, Globe, Pin, Gear, Save };
        enum class Editor { None, Defense, Damage, Movement, Jump, Falling, Tools, Ammo, Resources, Experience, SkillPoints };

        struct State
        {
            bool Chinese = true;
            bool InGame = false;
            bool GodMode = false;
            bool DemigodMode = false;
            bool InfiniteOxygen = false;
            bool MaxMedicinePouch = false;
            bool StealthMode = false;
            bool SuperDamage = false;
            bool InstantCharge = false;
            bool InfiniteWeaponStamina = false;
            bool InfiniteFocus = false;
            bool InfiniteValor = false;
            bool InfiniteSkillDuration = false;
            bool InfiniteReserveAmmo = false;
            bool InfiniteClipAmmo = false;
            bool InfiniteArrows = false;
            bool InfiniteJump = false;
            bool Noclip = false;
            bool FreeCamera = false;
            bool FreezeTrialTimer = false;
            bool AutoNeutralFaction = false;
            bool GameCompleted = false;
            bool ApplyPhotoModeInGame = false;
            bool FreeCrafting = false;
            bool ResourceIds = false;
            bool HardwareCursor = true;
            bool DamageMultiplier = false;
            bool DefenseMultiplier = false;
            bool MovementSpeed = false;
            bool FallSpeed = false;
            bool ExperienceMultiplier = false;
            bool PauseGame = false;
            bool PauseAI = false;
            bool PauseDayNight = false;
            bool DayNightCycle = true;
            bool TimescaleOverride = false;
            bool TimescaleInMenus = false;
            bool LodOverride = false;
            bool HasSavedPosition = false;
            bool HasUndoPosition = false;
            bool HasWaypoint = true;
            bool InventoryWindow = false;
            bool SpawnerWindow = false;
            bool WeatherWindow = false;
            bool LocationsWindow = false;
            bool LogWindow = false;
            bool DemoWindow = false;
            float Damage = 2.0f;
            float Defense = 2.0f;
            float Experience = 2.0f;
            float Speed = 3.0f;
            float Jump = 5.0f;
            float Falling = 0.5f;
            float Time = 12.0f;
            float Timescale = 1.0f;
            float LodBias = 1.0f;
            float InterfaceScale = 1.0f;
            int ToolsAmount = 1;
            int AmmoAmount = 1;
            int ResourcesAmount = 1;
            int SkillPoints = 1;
        };

        State g_State;
        Page g_Page = Page::Player;
        int g_Subtab = 0;
        Editor g_Editor = Editor::None;
        bool g_OpenEditor = false;
        float g_PageAnim = 1.0f;
        float g_Scale = 1.0f;
        float g_DpiScale = 1.0f;
        ImFont* g_Regular = nullptr;
        ImFont* g_Bold = nullptr;
        std::unordered_map<ImGuiID, float> g_Anim;

        float S(float value) { return value * g_Scale; }

        const char* T(const char* chinese, const char* english)
        {
            return g_State.Chinese ? chinese : english;
        }

        bool PageNeedsPlayer(Page page)
        {
            return page == Page::Player || page == Page::Resources;
        }

        float Ease(float current, float target, float speed)
        {
            const float t = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
            return current + (target - current) * std::clamp(t, 0.0f, 1.0f);
        }

        float& Anim(ImGuiID id, ImGuiID channel, float initial)
        {
            return g_Anim.try_emplace(id ^ (channel * 0x9E3779B9u), initial).first->second;
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                     a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
        }

        ImU32 Col(ImVec4 color, float alpha = 1.0f)
        {
            color.w *= alpha * ImGui::GetStyle().Alpha;
            return ImGui::GetColorU32(color);
        }

        void IconGlyph(ImDrawList* draw, Icon icon, ImVec2 center, ImU32 color, float scale = 1.0f)
        {
            const float s = g_Scale * scale;
            const float line = std::max(1.2f, 1.4f * s);
            switch (icon)
            {
            case Icon::User:
                draw->AddCircle(center - ImVec2(0, 4) * s, 3.0f * s, color, 16, line);
                draw->PathArcTo(center + ImVec2(0, 6) * s, 6.0f * s, IM_PI + .25f, IM_PI * 2.0f - .25f, 16);
                draw->PathStroke(color, 0, line);
                break;
            case Icon::Move:
                draw->AddLine(center - ImVec2(7, 0) * s, center + ImVec2(7, 0) * s, color, line);
                draw->AddTriangleFilled(center + ImVec2(8, 0) * s, center + ImVec2(3, -4) * s, center + ImVec2(3, 4) * s, color);
                break;
            case Icon::Bag:
                draw->AddRect(center - ImVec2(7, 5) * s, center + ImVec2(7, 7) * s, color, 2 * s, 0, line);
                draw->PathArcTo(center - ImVec2(0, 5) * s, 4 * s, IM_PI, IM_PI * 2, 12);
                draw->PathStroke(color, 0, line);
                break;
            case Icon::Globe:
                draw->AddCircle(center, 8 * s, color, 20, line);
                draw->AddLine(ImVec2(center.x, center.y - 7 * s), ImVec2(center.x, center.y + 7 * s), color, line);
                draw->AddLine(center - ImVec2(8, 0) * s, center + ImVec2(8, 0) * s, color, line);
                break;
            case Icon::Pin:
                draw->AddCircle(center - ImVec2(0, 3) * s, 5.5f * s, color, 18, line);
                draw->AddTriangle(center + ImVec2(-3.5f, 1) * s, center + ImVec2(3.5f, 1) * s, center + ImVec2(0, 9) * s, color, line);
                break;
            case Icon::Gear:
                draw->AddCircle(center, 7 * s, color, 18, line);
                draw->AddCircle(center, 2.5f * s, color, 14, line);
                for (int i = 0; i < 8; ++i)
                {
                    const float a = IM_PI * 2.0f * i / 8.0f;
                    const ImVec2 d(std::cos(a), std::sin(a));
                    draw->AddLine(center + d * (7 * s), center + d * (9 * s), color, line);
                }
                break;
            case Icon::Save:
                draw->AddRect(center - ImVec2(7, 7) * s, center + ImVec2(7, 7) * s, color, 2 * s, 0, line);
                draw->AddRect(center + ImVec2(-3, -7) * s, center + ImVec2(3, -1) * s, color, 0, 0, line);
                draw->AddCircle(center + ImVec2(0, 3) * s, 2.3f * s, color, 12, line);
                break;
            }
        }

        bool FlatButton(const char* idText, const char* text, ImVec2 size, Icon* icon = nullptr)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect bb(w->DC.CursorPos, w->DC.CursorPos + size);
            ImGui::ItemSize(bb);
            if (!ImGui::ItemAdd(bb, id)) return false;
            bool hovered = false, held = false;
            const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
            float& h = Anim(id, 1, 0.0f);
            h = Ease(h, hovered ? 1.0f : 0.0f, 18.0f);
            w->DrawList->AddRectFilled(bb.Min, bb.Max, Col(held ? Theme::FrameOn : Mix(Theme::Button, Theme::ButtonHover, h)), S(4));
            w->DrawList->AddRect(bb.Min, bb.Max, Col(Theme::Border), S(4));
            const ImVec4 fg = Mix(Theme::Muted, Theme::Text, h);
            float offset = 0;
            if (icon)
            {
                IconGlyph(w->DrawList, *icon, bb.Min + ImVec2(S(17), bb.GetHeight() * .5f), Col(fg), .76f);
                offset = S(12);
            }
            const ImVec2 ts = ImGui::CalcTextSize(text);
            w->DrawList->AddText(bb.GetCenter() - ts * .5f + ImVec2(offset, 0), Col(fg), text);
            return pressed;
        }

        bool Nav(const char* idText, Icon icon, const char* label, Page page, bool enabled = true)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect bb(w->DC.CursorPos, w->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, S(38)));
            ImGui::ItemSize(bb);
            if (!ImGui::ItemAdd(bb, id)) return false;
            bool hovered = false, held = false;
            const bool pressed = enabled && ImGui::ButtonBehavior(bb, id, &hovered, &held);
            const bool selected = page == g_Page;
            float& a = Anim(id, 2, selected ? 1.0f : 0.0f);
            float& h = Anim(id, 3, 0.0f);
            a = Ease(a, selected ? 1.0f : 0.0f, 12.0f);
            h = Ease(h, hovered ? 1.0f : 0.0f, 18.0f);
            w->DrawList->AddRectFilled(bb.Min, bb.Max, Col(Theme::FrameOn, a * .82f + h * .18f), S(5));
            const float lit = enabled ? std::max(a, h * .6f) : 0.0f;
            IconGlyph(w->DrawList, icon, bb.Min + ImVec2(S(18), S(19)), Col(Mix(Theme::Muted, Theme::Accent, lit)), .82f);
            const float navFontSize = g_Regular ? g_Regular->FontSize : ImGui::GetFontSize();
            const ImVec2 ts = g_Bold ? g_Bold->CalcTextSizeA(navFontSize, FLT_MAX, 0.0f, label) : ImGui::CalcTextSize(label);
            ImVec4 textColor = Mix(Theme::Muted, Theme::Text, lit);
            if (!enabled) textColor.w = .38f;
            const ImVec2 textPos = bb.Min + ImVec2(S(40), (bb.GetHeight() - ts.y) * .5f);
            if (g_Bold) w->DrawList->AddText(g_Bold, navFontSize, textPos, Col(textColor), label);
            else w->DrawList->AddText(textPos, Col(textColor), label);
            if (!enabled)
            {
                const ImVec2 lockCenter(bb.Max.x - S(13), bb.GetCenter().y + S(1));
                w->DrawList->AddRect(lockCenter - ImVec2(S(4), S(2)), lockCenter + ImVec2(S(4), S(5)), Col(Theme::Muted, .34f), S(1));
                w->DrawList->PathArcTo(lockCenter - ImVec2(0, S(2)), S(3), IM_PI, IM_PI * 2.0f, 10);
                w->DrawList->PathStroke(Col(Theme::Muted, .34f), 0, std::max(1.0f, S(1.1f)));
                if (ImGui::IsMouseHoveringRect(bb.Min, bb.Max))
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(g_State.Chinese ? "进入游戏并创建玩家实体后可用" : "Available after entering the game");
                    ImGui::EndTooltip();
                }
            }
            if (pressed && !selected)
            {
                g_Page = page;
                g_Subtab = 0;
                g_PageAnim = 0;
            }
            return pressed;
        }

        void Category(const char* text)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + S(8));
            ImGui::TextColored(Theme::Muted, "%s", text);
            ImGui::Dummy(ImVec2(0, S(3)));
        }

        bool Subtab(const char* idText, const char* text, bool selected, ImVec2 size, ImDrawFlags rounding)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect bb(w->DC.CursorPos, w->DC.CursorPos + size);
            ImGui::ItemSize(bb);
            if (!ImGui::ItemAdd(bb, id)) return false;
            bool hovered = false, held = false;
            const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
            float& a = Anim(id, 4, selected ? 1.0f : 0.0f);
            float& h = Anim(id, 5, 0.0f);
            a = Ease(a, selected ? 1.0f : 0.0f, 13.0f);
            h = Ease(h, hovered ? 1.0f : 0.0f, 18.0f);
            w->DrawList->AddRectFilled(bb.Min, bb.Max, Col(Theme::FrameOn, a * .82f + h * .14f), S(4), rounding);
            const float fontSize = g_Regular ? g_Regular->FontSize : ImGui::GetFontSize();
            const ImVec2 ts = g_Bold ? g_Bold->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text) : ImGui::CalcTextSize(text);
            if (g_Bold) w->DrawList->AddText(g_Bold, fontSize, bb.GetCenter() - ts * .5f, Col(Mix(Theme::Muted, Theme::Text, std::max(a, h * .7f))), text);
            else w->DrawList->AddText(bb.GetCenter() - ts * .5f, Col(Mix(Theme::Muted, Theme::Text, std::max(a, h * .7f))), text);
            return pressed;
        }

        void BeginBox(const char* idText, const char* title, ImVec2 size)
        {
            ImGui::PushID(idText);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::BeginChild("##outer", size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* d = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetWindowPos();
            const ImVec2 z = ImGui::GetWindowSize();
            d->AddRectFilled(p + ImVec2(0, S(13)), p + z, Col(Theme::Group), S(6));
            d->AddRect(p + ImVec2(0, S(13)), p + z, Col(Theme::Border), S(6));
            const ImVec2 ts = ImGui::CalcTextSize(title);
            d->AddRectFilled(p + ImVec2(S(9), S(5)), p + ImVec2(S(18) + ts.x, S(22)), Col(Theme::Window));
            if (g_Bold) d->AddText(g_Bold, ImGui::GetFontSize(), p + ImVec2(S(13), 0), Col(Theme::Muted, .82f), title);
            else d->AddText(p + ImVec2(S(13), 0), Col(Theme::Muted, .78f), title);
            ImGui::SetCursorPos(ImVec2(S(12), S(30)));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, S(4)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8), S(10)));
            ImGui::BeginChild("##inner", ImVec2(z.x - S(24), z.y - S(36)), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        }

        void EndBox()
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        bool Toggle(const char* idText, const char* text, bool* value)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect row(w->DC.CursorPos, w->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, S(24)));
            const ImRect sw(row.Max - ImVec2(S(34), S(21.5f)), row.Max - ImVec2(0, S(2.5f)));
            ImGui::ItemSize(row);
            if (!ImGui::ItemAdd(row, id)) return false;
            bool hovered = false, held = false;
            const bool pressed = ImGui::ButtonBehavior(row, id, &hovered, &held);
            if (pressed) { *value = !*value; ImGui::MarkItemEdited(id); }
            float& t = Anim(id, 6, *value ? 1.0f : 0.0f);
            float& h = Anim(id, 7, 0.0f);
            t = Ease(t, *value ? 1.0f : 0.0f, 14.0f);
            h = Ease(h, hovered ? 1.0f : 0.0f, 20.0f);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            w->DrawList->AddText(ImVec2(row.Min.x, row.GetCenter().y - ts.y * .5f), Col(*value ? Theme::Text : Mix(Theme::Muted, Theme::Text, h * .5f)), text);
            w->DrawList->AddRectFilled(sw.Min, sw.Max, Col(Theme::FrameOff), sw.GetHeight() * .5f);
            w->DrawList->AddRectFilled(sw.Min, sw.Max, Col(Theme::FrameOn, t), sw.GetHeight() * .5f);
            w->DrawList->AddRect(sw.Min, sw.Max, Col(Theme::Border), sw.GetHeight() * .5f);
            const float knobX = sw.Min.x + S(9) + (sw.GetWidth() - S(18)) * t;
            w->DrawList->AddCircleFilled(ImVec2(knobX, sw.GetCenter().y), S(7), Col(Mix(Theme::Muted, Theme::Accent, t)), 24);
            return pressed;
        }

        bool Slider(const char* idText, const char* text, float* value, float minimum, float maximum, const char* format)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect row(w->DC.CursorPos, w->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, S(27)));
            const float valueWidth = S(48), trackWidth = std::min(S(126), row.GetWidth() * .42f);
            const ImRect track(ImVec2(row.Max.x - valueWidth - S(12) - trackWidth, row.GetCenter().y - S(2)), ImVec2(row.Max.x - valueWidth - S(12), row.GetCenter().y + S(2)));
            const ImRect valueBox(ImVec2(row.Max.x - valueWidth, row.Min.y + S(2)), row.Max - ImVec2(0, S(2)));
            ImGui::ItemSize(row);
            if (!ImGui::ItemAdd(row, id)) return false;
            bool hovered = false, held = false;
            ImGui::ButtonBehavior(track, id, &hovered, &held);
            bool changed = false;
            if (held && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const float ratio = std::clamp((ImGui::GetIO().MousePos.x - track.Min.x) / track.GetWidth(), 0.0f, 1.0f);
                const float next = minimum + (maximum - minimum) * ratio;
                changed = std::abs(next - *value) > .0001f;
                *value = next;
            }
            const float ratio = std::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
            float& fill = Anim(id, 8, ratio);
            fill = Ease(fill, ratio, 16.0f);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            w->DrawList->AddText(ImVec2(row.Min.x, row.GetCenter().y - ts.y * .5f), Col(Theme::Muted), text);
            w->DrawList->AddRectFilled(track.Min, track.Max, Col(Theme::FrameOn), S(4));
            w->DrawList->AddRectFilled(track.Min, ImVec2(track.Min.x + track.GetWidth() * fill, track.Max.y), Col(Theme::Accent), S(4));
            w->DrawList->AddCircleFilled(ImVec2(track.Min.x + track.GetWidth() * fill, track.GetCenter().y), S(5.5f), Col(Theme::Accent), 20);
            w->DrawList->AddRectFilled(valueBox.Min, valueBox.Max, Col(Theme::FrameOff), S(4));
            w->DrawList->AddRect(valueBox.Min, valueBox.Max, Col(Theme::Border), S(4));
            char buffer[32] {}; std::snprintf(buffer, sizeof(buffer), format, *value);
            const ImVec2 vs = ImGui::CalcTextSize(buffer);
            w->DrawList->AddText(valueBox.GetCenter() - vs * .5f, Col(Theme::Text), buffer);
            return changed;
        }

        bool Combo(const char* idText, const char* text, int* selected, const char* const* items, int count)
        {
            ImGui::PushID(idText);
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID("##combo");
            const ImRect row(w->DC.CursorPos, w->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, S(29)));
            const float boxWidth = std::min(S(170), row.GetWidth() * .54f);
            const ImRect box(ImVec2(row.Max.x - boxWidth, row.Min.y), row.Max);
            ImGui::ItemSize(row); ImGui::ItemAdd(row, id);
            bool hovered = false, held = false;
            if (ImGui::ButtonBehavior(box, id, &hovered, &held)) ImGui::OpenPopup("##popup");
            float& h = Anim(id, 9, 0); h = Ease(h, hovered ? .55f : 0, 16);
            w->DrawList->AddText(ImVec2(row.Min.x, row.GetCenter().y - ImGui::CalcTextSize(text).y * .5f), Col(Theme::Muted), text);
            w->DrawList->AddRectFilled(box.Min, box.Max, Col(Mix(Theme::FrameOff, Theme::FrameOn, h)), S(4));
            w->DrawList->AddRect(box.Min, box.Max, Col(Theme::Border), S(4));
            const char* preview = *selected >= 0 && *selected < count ? items[*selected] : "-";
            w->DrawList->AddText(ImVec2(box.Min.x + S(10), box.GetCenter().y - ImGui::CalcTextSize(preview).y * .5f), Col(Theme::Muted), preview);
            const ImVec2 a(box.Max.x - S(14), box.GetCenter().y);
            w->DrawList->AddTriangleFilled(a + ImVec2(-S(4), -S(2)), a + ImVec2(S(4), -S(2)), a + ImVec2(0, S(3)), Col(Theme::Text));
            bool changed = false;
            ImGui::SetNextWindowSizeConstraints(ImVec2(boxWidth, 0), ImVec2(boxWidth, S(240)));
            if (ImGui::BeginPopup("##popup"))
            {
                for (int i = 0; i < count; ++i)
                    if (ImGui::Selectable(items[i], *selected == i, 0, ImVec2(0, S(28)))) { *selected = i; changed = true; }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            return changed;
        }

        bool ActionRow(const char* idText, const char* text, const char* value, bool enabled = true)
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            const ImGuiID id = w->GetID(idText);
            const ImRect bb(w->DC.CursorPos, w->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, S(29)));
            ImGui::ItemSize(bb);
            if (!ImGui::ItemAdd(bb, id)) return false;
            bool hovered = false, held = false;
            const bool pressed = enabled && ImGui::ButtonBehavior(bb, id, &hovered, &held);
            float& h = Anim(id, 12, 0.0f);
            h = Ease(h, hovered && enabled ? 1.0f : 0.0f, 18.0f);
            w->DrawList->AddRectFilled(bb.Min, bb.Max, Col(Mix(Theme::Group, Theme::FrameOn, h * .72f)), S(4));
            w->DrawList->AddRect(bb.Min, bb.Max, Col(Theme::Border), S(4));
            ImVec4 labelColor = enabled ? Theme::Text : Theme::Muted;
            if (!enabled) labelColor.w = .38f;
            w->DrawList->AddText(bb.Min + ImVec2(S(9), (bb.GetHeight() - ImGui::GetFontSize()) * .5f), Col(labelColor), text);
            const ImVec2 valueSize = ImGui::CalcTextSize(value);
            const ImVec2 valuePos(bb.Max.x - valueSize.x - S(20), bb.GetCenter().y - valueSize.y * .5f);
            w->DrawList->AddText(valuePos, Col(enabled ? Mix(Theme::Muted, Theme::Accent, h * .6f) : Theme::Muted, enabled ? 1.0f : .38f), value);
            const ImVec2 arrow(bb.Max.x - S(8), bb.GetCenter().y);
            w->DrawList->AddTriangleFilled(arrow + ImVec2(-S(3), -S(4)), arrow + ImVec2(-S(3), S(4)), arrow + ImVec2(S(2), 0), Col(Theme::Muted, enabled ? .85f : .25f));
            return pressed;
        }

        void OpenEditor(Editor editor)
        {
            g_Editor = editor;
            g_OpenEditor = true;
        }

        bool* EditorEnabledFlag()
        {
            switch (g_Editor)
            {
            case Editor::Defense: return &g_State.DefenseMultiplier;
            case Editor::Damage: return &g_State.DamageMultiplier;
            case Editor::Movement: return &g_State.MovementSpeed;
            case Editor::Jump: return &g_State.InfiniteJump;
            case Editor::Falling: return &g_State.FallSpeed;
            case Editor::Experience: return &g_State.ExperienceMultiplier;
            default: return nullptr;
            }
        }

        float* EditorFloatValue()
        {
            switch (g_Editor)
            {
            case Editor::Defense: return &g_State.Defense;
            case Editor::Damage: return &g_State.Damage;
            case Editor::Movement: return &g_State.Speed;
            case Editor::Jump: return &g_State.Jump;
            case Editor::Falling: return &g_State.Falling;
            case Editor::Experience: return &g_State.Experience;
            default: return nullptr;
            }
        }

        int* EditorIntegerValue()
        {
            switch (g_Editor)
            {
            case Editor::Tools: return &g_State.ToolsAmount;
            case Editor::Ammo: return &g_State.AmmoAmount;
            case Editor::Resources: return &g_State.ResourcesAmount;
            case Editor::SkillPoints: return &g_State.SkillPoints;
            default: return nullptr;
            }
        }

        const char* EditorTitle()
        {
            switch (g_Editor)
            {
            case Editor::Defense: return T("防御倍率设置", "Defense multiplier");
            case Editor::Damage: return T("伤害倍率设置", "Damage multiplier");
            case Editor::Movement: return T("移动速度设置", "Movement speed");
            case Editor::Jump: return T("无限跳高度设置", "Infinite jump height");
            case Editor::Falling: return T("下降速度设置", "Falling speed");
            case Editor::Tools: return T("修改工具数量", "Edit tools amount");
            case Editor::Ammo: return T("修改弹药数量", "Edit ammunition amount");
            case Editor::Resources: return T("修改资源数量", "Edit resources amount");
            case Editor::Experience: return T("经验倍率设置", "Experience multiplier");
            case Editor::SkillPoints: return T("修改技能点", "Edit skill points");
            default: return T("数值设置", "Value editor");
            }
        }

        const char* EnabledValue(bool enabled, float value, int decimals = 1)
        {
            static thread_local char buffer[32];
            if (!enabled) return T("关闭", "Off");
            std::snprintf(buffer, sizeof(buffer), decimals == 2 ? "%.2fx" : "%.1fx", value);
            return buffer;
        }

        void Line()
        {
            ImGui::Dummy(ImVec2(0, S(1)));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(p, p + ImVec2(ImGui::GetContentRegionAvail().x, 0), Col(Theme::Border));
            ImGui::Dummy(ImVec2(0, S(1)));
        }

        float LobbyNotice()
        {
            const float height = S(42);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 z(ImGui::GetContentRegionAvail().x, height);
            ImDrawList* d = ImGui::GetWindowDrawList();
            const ImVec4 warning(.95f, .65f, .24f, 1.0f);
            d->AddRectFilled(p, p + z, Col(ImVec4(.17f, .105f, .035f, .72f)), S(5));
            d->AddRect(p, p + z, Col(warning, .28f), S(5));
            d->AddCircleFilled(p + ImVec2(S(19), height * .5f), S(4), Col(warning));
            d->AddText(p + ImVec2(S(32), S(7)), Col(warning), g_State.Chinese ? "当前处于大厅模拟状态" : "Lobby simulation is active");
            d->AddText(p + ImVec2(S(32), S(23)), Col(Theme::Muted, .72f), g_State.Chinese ? "玩家实体尚未创建，以下功能不可操作。" : "Player entity is unavailable; controls below are disabled.");
            ImGui::Dummy(z);
            ImGui::Dummy(ImVec2(0, S(4)));
            return height + S(4);
        }

        void TwoColumns(ImVec2 size, const std::function<void(float)>& left, const std::function<void(float)>& right)
        {
            const float gap = S(10), width = (size.x - gap) * .5f;
            ImGui::BeginGroup(); left(width); ImGui::EndGroup();
            ImGui::SameLine(0, gap);
            ImGui::BeginGroup(); right(width); ImGui::EndGroup();
        }

        void PlayerPage(ImVec2 size)
        {
            TwoColumns(size,
                [&](float w)
                {
                    if (g_Subtab == 0)
                    {
                        BeginBox("survival", T("生存状态", "SURVIVAL"), ImVec2(w, size.y));
                        if (Toggle("god", T("完全无敌", "God mode"), &g_State.GodMode) && g_State.GodMode) g_State.DemigodMode = false;
                        if (Toggle("demigod", T("半无敌模式", "Demigod mode"), &g_State.DemigodMode) && g_State.DemigodMode) g_State.GodMode = false;
                        if (ActionRow("defense", T("防御倍率设置", "Defense multiplier"), EnabledValue(g_State.DefenseMultiplier, g_State.Defense))) OpenEditor(Editor::Defense);
                        Toggle("oxygen", T("无限氧气", "Infinite oxygen"), &g_State.InfiniteOxygen);
                        Toggle("medicine", T("药用浆果袋保持满额", "Keep medicine pouch full"), &g_State.MaxMedicinePouch);
                        Toggle("stealth", T("隐身模式", "Stealth mode"), &g_State.StealthMode);
                        EndBox();
                    }
                    else if (g_Subtab == 1)
                    {
                        BeginBox("combat", T("战斗与能力", "COMBAT & ABILITIES"), ImVec2(w, size.y));
                        Toggle("super-damage", T("超级伤害 / 一击必杀", "Super damage / one-hit kill"), &g_State.SuperDamage);
                        if (ActionRow("damage-mult", T("伤害倍率设置", "Damage multiplier"), EnabledValue(g_State.DamageMultiplier, g_State.Damage))) OpenEditor(Editor::Damage);
                        Toggle("charge", T("弓箭瞬间蓄力", "Instant bow charge"), &g_State.InstantCharge);
                        Toggle("weapon-stamina", T("无限武器耐力", "Infinite weapon stamina"), &g_State.InfiniteWeaponStamina);
                        Toggle("focus", T("无限专注", "Infinite focus"), &g_State.InfiniteFocus);
                        Toggle("valor", T("无限勇气", "Infinite valor"), &g_State.InfiniteValor);
                        Toggle("skill-duration", T("无限技能持续时间", "Infinite skill duration"), &g_State.InfiniteSkillDuration);
                        EndBox();
                    }
                    else if (g_Subtab == 2)
                    {
                        BeginBox("movement", T("移动与探索", "MOVEMENT & EXPLORATION"), ImVec2(w, size.y));
                        if (ActionRow("move-speed", T("移动速度设置", "Movement speed"), EnabledValue(g_State.MovementSpeed, g_State.Speed))) OpenEditor(Editor::Movement);
                        if (ActionRow("jump-height", T("无限跳高度设置", "Infinite jump height"), EnabledValue(g_State.InfiniteJump, g_State.Jump))) OpenEditor(Editor::Jump);
                        if (ActionRow("fall-speed", T("下降速度设置", "Falling speed"), EnabledValue(g_State.FallSpeed, g_State.Falling, 2))) OpenEditor(Editor::Falling);
                        Toggle("noclip", T("穿墙模式（丶 / ~）", "Noclip (` / ~)"), &g_State.Noclip);
                        Toggle("free-camera", T("自由镜头", "Free camera"), &g_State.FreeCamera);
                        EndBox();
                    }
                    else
                    {
                        BeginBox("special", T("特殊与调试", "SPECIAL & DEBUG"), ImVec2(w, size.y));
                        Toggle("trial", T("锁定试炼时间", "Freeze trial timer"), &g_State.FreezeTrialTimer);
                        Toggle("neutral", T("自动中立阵营", "Automatic neutral faction"), &g_State.AutoNeutralFaction);
                        Toggle("completed", T("模拟游戏已完成", "Simulate game completed"), &g_State.GameCompleted);
                        Toggle("photo", T("游戏中应用拍照模式设置", "Apply photo settings in game"), &g_State.ApplyPhotoModeInGame);
                        EndBox();
                    }
                },
                [&](float w)
                {
                    if (g_Subtab == 1)
                    {
                        BeginBox("ammo", T("弹药", "AMMUNITION"), ImVec2(w, size.y));
                        if (Toggle("reserve-ammo", T("无限备用弹药", "Infinite reserve ammo"), &g_State.InfiniteReserveAmmo) && g_State.InfiniteReserveAmmo) g_State.InfiniteClipAmmo = false;
                        if (Toggle("clip-ammo", T("无限弹匣弹药", "Infinite clip ammo"), &g_State.InfiniteClipAmmo) && g_State.InfiniteClipAmmo) g_State.InfiniteReserveAmmo = false;
                        Toggle("arrows", T("无限箭矢与陷阱", "Infinite arrows and traps"), &g_State.InfiniteArrows);
                        Line();
                        ImGui::TextWrapped("%s", T("备用弹药与弹匣弹药互斥；箭矢与陷阱可同时开启。", "Reserve and clip ammo are mutually exclusive; arrows and traps may remain enabled."));
                        EndBox();
                    }
                    else if (g_Subtab == 2)
                    {
                        BeginBox("hotkeys", T("操作说明", "CONTROLS"), ImVec2(w, size.y));
                        ImGui::TextColored(Theme::Muted, "INS"); ImGui::SameLine(S(105)); ImGui::TextUnformatted(T("显示 / 隐藏菜单", "Show / hide menu")); Line();
                        ImGui::TextColored(Theme::Muted, "` / ~"); ImGui::SameLine(S(105)); ImGui::TextUnformatted(T("穿墙模式", "Noclip mode")); Line();
                        ImGui::TextColored(Theme::Muted, "SPACE"); ImGui::SameLine(S(105)); ImGui::TextUnformatted(T("踏空跳 / 上升", "Air jump / ascend"));
                        EndBox();
                    }
                    else
                    {
                        BeginBox("status", T("状态与说明", "STATUS & NOTES"), ImVec2(w, size.y));
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", T("DLL 已动态加载", "DLL loaded dynamically"));
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", T("DirectX 12 渲染正常", "DirectX 12 renderer ready"));
                        ImGui::TextColored(Theme::Muted, "%s", T("所有修改项默认关闭", "All modifiers default to off"));
                        ImGui::TextColored(Theme::Muted, "FPS  %.0f", ImGui::GetIO().Framerate);
                        Line();
                        ImGui::TextWrapped("%s", T("预览只保存本地模拟状态，不读取或修改游戏内存。", "The preview only keeps local mock state and never accesses game memory."));
                        EndBox();
                    }
                });
        }

        void StandardPage(ImVec2 size)
        {
            TwoColumns(size,
                [&](float w)
                {
                    BeginBox("main", T("主要设置", "MAIN"), ImVec2(w, size.y));
                    if (g_Page == Page::Resources)
                    {
                        if (ActionRow("inventory", T("玩家物品栏", "Player inventory"), T("打开", "Open"))) g_State.InventoryWindow = true;
                        if (ActionRow("tools", T("修改工具数量", "Edit tools amount"), T("未启用", "Not active"))) OpenEditor(Editor::Tools);
                        if (ActionRow("ammo", T("修改弹药数量", "Edit ammunition amount"), T("未启用", "Not active"))) OpenEditor(Editor::Ammo);
                        if (ActionRow("resources", T("修改资源数量", "Edit resources amount"), T("未启用", "Not active"))) OpenEditor(Editor::Resources);
                        Toggle("craft", T("无视制作与购买需求", "Ignore crafting and purchase costs"), &g_State.FreeCrafting);
                        if (ActionRow("experience", T("经验倍率设置", "Experience multiplier"), EnabledValue(g_State.ExperienceMultiplier, g_State.Experience))) OpenEditor(Editor::Experience);
                        FlatButton("grant-exp", T("获得大量经验", "Grant large amount of XP"), ImVec2(ImGui::GetContentRegionAvail().x, S(33)));
                        if (ActionRow("skill-points", T("修改技能点", "Edit skill points"), T("未启用", "Not active"))) OpenEditor(Editor::SkillPoints);
                    }
                    else if (g_Page == Page::World)
                    {
                        Toggle("pause-game", T("暂停游戏逻辑", "Pause game logic"), &g_State.PauseGame);
                        Toggle("pause-ai", T("暂停 AI 处理", "Pause AI processing"), &g_State.PauseAI);
                        ImGui::BeginDisabled(!g_State.InGame);
                        Toggle("pause-day", T("暂停昼夜时间", "Pause day/night time"), &g_State.PauseDayNight);
                        Toggle("day-cycle", T("启用昼夜循环", "Enable day/night cycle"), &g_State.DayNightCycle);
                        Slider("time", T("当前时间", "Current time"), &g_State.Time, 0, 24, "%.1fh");
                        ImGui::EndDisabled();
                        Toggle("timescale-enable", T("时间倍率覆盖", "Timescale override"), &g_State.TimescaleOverride);
                        Toggle("timescale-menus", T("菜单内保持时间倍率", "Keep timescale in menus"), &g_State.TimescaleInMenus);
                        ImGui::BeginDisabled(!g_State.TimescaleOverride);
                        Slider("timescale", T("时间倍率", "Timescale"), &g_State.Timescale, .1f, 5, "%.2fx");
                        ImGui::EndDisabled();
                        Toggle("lod-enable", T("LOD 偏差覆盖", "LOD bias override"), &g_State.LodOverride);
                        ImGui::BeginDisabled(!g_State.LodOverride);
                        Slider("lod", T("LOD 偏差", "LOD bias"), &g_State.LodBias, 0, 1, "%.2f");
                        ImGui::EndDisabled();
                    }
                    else if (g_Page == Page::Developer)
                    {
                        if (ActionRow("log", T("显示日志窗口", "Show log window"), T("打开", "Open"))) g_State.LogWindow = true;
                        if (ActionRow("demo", T("显示 ImGui 演示窗口", "Show ImGui demo"), T("打开", "Open"))) g_State.DemoWindow = true;
                        ActionRow("rtti", T("导出 RTTI 结构", "Export RTTI structures"), T("执行", "Run"));
                        ActionRow("components", T("导出玩家组件", "Export player components"), T("执行", "Run"), g_State.InGame);
                    }
                    else
                    {
                        Toggle("hardware", T("硬件鼠标光标", "Hardware mouse cursor"), &g_State.HardwareCursor);
                        Slider("ui", T("界面缩放", "Interface scale"), &g_State.InterfaceScale, .85f, 1.5f, "%.2fx");
                    }
                    EndBox();
                },
                [&](float w)
                {
                    BeginBox("secondary", g_Page == Page::World ? T("传送与工具", "TELEPORT & TOOLS") : T("状态与预览", "STATUS & PREVIEW"), ImVec2(w, size.y));
                    if (g_Page == Page::World)
                    {
                        ActionRow("quick-save", T("强制快速保存", "Force quick save"), T("执行", "Run"), g_State.InGame);
                        ActionRow("quick-load", T("读取上一存档", "Load previous save"), T("执行", "Run"), g_State.InGame);
                        if (ActionRow("save-position", T("保存当前位置", "Save current position"), T("执行", "Run"), g_State.InGame)) g_State.HasSavedPosition = true;
                        ActionRow("saved-position", T("传送到保存位置", "Teleport to saved position"), T("执行", "Run"), g_State.InGame && g_State.HasSavedPosition);
                        ActionRow("undo-teleport", T("撤销上次传送", "Undo last teleport"), T("执行", "Run"), g_State.InGame && g_State.HasUndoPosition);
                        if (ActionRow("waypoint", T("传送到地图标记点", "Teleport to waypoint"), T("执行", "Run"), g_State.InGame && g_State.HasWaypoint)) g_State.HasUndoPosition = true;
                        ActionRow("camera-position", T("传送到自由镜头位置", "Teleport to free-camera position"), T("执行", "Run"), g_State.InGame);
                        if (ActionRow("locations", T("预设地点", "Preset locations"), T("打开", "Open"), g_State.InGame)) g_State.LocationsWindow = true;
                        if (ActionRow("spawner", T("实体生成器", "Entity spawner"), T("打开", "Open"), g_State.InGame)) g_State.SpawnerWindow = true;
                        if (ActionRow("weather", T("天气设置", "Weather setup"), T("打开", "Open"))) g_State.WeatherWindow = true;
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", T("DLL 已加载", "DLL loaded"));
                        ImGui::TextColored(Theme::Muted, "%s", T("仅使用本地模拟状态", "Local mock state only"));
                        Line();
                        ImGui::TextWrapped("%s", T("本页用于检查控件比例、字体、弹层和动画，不访问游戏内存。", "This page tests control proportions, typography, popups and animation without game memory access."));
                        if (g_Page == Page::Developer)
                        {
                            Line();
                            ImGui::TextUnformatted(T("项目版本", "Project version"));
                            ImGui::TextColored(Theme::Muted, "0.18 / HFW Gameplay Tweaks");
                        }
                    }
                    EndBox();
                });
        }

        void RenderValueEditor()
        {
            if (g_OpenEditor)
            {
                ImGui::OpenPopup("##value-editor");
                g_OpenEditor = false;
            }

            ImGui::SetNextWindowSize(ImVec2(S(520), 0), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Appearing, ImVec2(.5f, .5f));
            if (!ImGui::BeginPopupModal("##value-editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
                return;

            if (g_Bold)
            {
                ImGui::PushFont(g_Bold);
                ImGui::TextUnformatted(EditorTitle());
                ImGui::PopFont();
            }
            else ImGui::TextUnformatted(EditorTitle());
            ImGui::Separator();

            if (float* value = EditorFloatValue())
            {
                float minimum = 1.0f, maximum = 100.0f;
                if (g_Editor == Editor::Movement) { minimum = .1f; maximum = 10.0f; }
                else if (g_Editor == Editor::Jump) { minimum = 1.0f; maximum = 50.0f; }
                else if (g_Editor == Editor::Falling) { minimum = .1f; maximum = 5.0f; }
                ImGui::TextWrapped("%s", g_Editor == Editor::Jump
                    ? T("输入高度倍率；1 为原版，默认输入 5。确认后才启用无限跳。", "Enter a height multiplier; 1 is vanilla and 5 is the preset. Infinite jump is enabled only after confirmation.")
                    : g_Editor == Editor::Movement
                        ? T("输入移动速度倍率；1 为原版，默认输入 3。确认后才启用。", "Enter a movement multiplier; 1 is vanilla and 3 is the preset. It is enabled only after confirmation.")
                        : g_Editor == Editor::Falling
                            ? T("输入下降速度倍率；1 为原版，默认输入 0.5。确认后才启用。", "Enter a falling multiplier; 1 is vanilla and 0.5 is the preset. It is enabled only after confirmation.")
                            : T("输入倍率并确认后才会启用；关闭后恢复游戏默认计算。", "The multiplier is enabled only after confirmation; disabling restores the game calculation."));
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputFloat("##float-value", value, .1f, 1.0f, "%.2f");
                *value = std::clamp(*value, minimum, maximum);
            }
            else if (int* value = EditorIntegerValue())
            {
                ImGui::TextWrapped("%s", T("输入目标数量并确认。数量修改只执行一次，不会持续锁定。", "Enter a target amount and confirm. The change runs once and is not locked."));
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputInt("##integer-value", value, 1, 10);
                *value = std::clamp(*value, 1, g_Editor == Editor::SkillPoints ? 9999 : 999999);
            }
            ImGui::Spacing();
            const float gap = S(10), buttonWidth = (ImGui::GetContentRegionAvail().x - gap) * .5f;
            if (ImGui::Button(T("确认", "Confirm"), ImVec2(buttonWidth, S(38))))
            {
                if (bool* enabled = EditorEnabledFlag()) *enabled = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, gap);
            const bool active = EditorEnabledFlag() && *EditorEnabledFlag();
            if (ImGui::Button(active ? T("关闭修改", "Disable") : T("取消", "Cancel"), ImVec2(buttonWidth, S(38))))
            {
                if (bool* enabled = EditorEnabledFlag()) *enabled = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        void RenderInventoryWindow()
        {
            if (!g_State.InventoryWindow) return;
            static char filter[96] {};
            static bool onlyOwned = true;
            static bool localized = true;
            static bool showIds = false;
            static int selected = -1;
            static int editCount = 0;
            static std::array<int, 8> counts { 24, 2, 1, 24, 361, 50, 6, 2 };
            constexpr std::array<const char*, 8> namesZh { "编织线", "苦叶", "药剂袋", "药用浆果", "金属碎片", "金属腐蚀罐", "金属骨", "铁锭" };
            constexpr std::array<const char*, 8> namesEn { "Braided Wire", "Bitter Leaf", "Potion Pouch", "Medicinal Berry", "Metal Shards", "Metalbite Sac", "Metal Bone", "Iron Ingot" };

            ImGui::SetNextWindowSize(ImVec2(S(900), S(650)), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
            if (ImGui::Begin(T("玩家物品栏###InventoryPreview", "Player inventory###InventoryPreview"), &g_State.InventoryWindow, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(ImVec4(1, .74f, .25f, 1), "%s", T("警告：生成、添加或删除任务物品可能永久破坏游戏进度。", "Warning: adding or removing quest items may permanently damage progression."));
                ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputTextWithHint("##inventory-filter", T("筛选（包含、-排除）", "Filter (include, -exclude)"), filter, sizeof(filter));
                ImGui::Checkbox(T("仅显示玩家物品栏中的物品", "Only show owned items"), &onlyOwned); ImGui::SameLine();
                ImGui::Checkbox(T("显示游戏本地化名称", "Show localized names"), &localized); ImGui::SameLine();
                ImGui::Checkbox(T("显示内部资源 ID", "Show internal resource IDs"), &showIds);
                ImGui::TextColored(ImVec4(.55f, .82f, 1, 1), "%s", T("单击物品行可输入目标总数量；输入 0 会删除该物品。", "Click an item row to enter its target total; enter 0 to remove it."));
                const int columns = showIds ? 3 : 2;
                if (ImGui::BeginTable("##inventory-table", columns, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn(T("名称", "Name"), ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn(T("数量", "Count"), ImGuiTableColumnFlags_WidthFixed, S(90));
                    if (showIds) ImGui::TableSetupColumn(T("内部资源 ID", "Internal resource ID"), ImGuiTableColumnFlags_WidthFixed, S(260));
                    ImGui::TableHeadersRow();
                    for (int i = 0; i < static_cast<int>(counts.size()); ++i)
                    {
                        ImGui::PushID(i); ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                        const char* name = g_State.Chinese ? namesZh[i] : namesEn[i];
                        if (ImGui::Selectable(name, false, ImGuiSelectableFlags_SpanAllColumns)) { selected = i; editCount = counts[i]; ImGui::OpenPopup("##inventory-count"); }
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%d", counts[i]);
                        if (showIds) { ImGui::TableSetColumnIndex(2); ImGui::Text("%08X-0000-4000-8000-%012X", i + 1, i + 0x100); }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::SetNextWindowSize(ImVec2(S(440), 0), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("##inventory-count", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("%s: %s", T("物品", "Item"), selected >= 0 ? (g_State.Chinese ? namesZh[selected] : namesEn[selected]) : "-");
                    ImGui::TextUnformatted(T("输入目标总数量（0 - 999999）", "Enter target total (0 - 999999)"));
                    ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt("##target-count", &editCount, 1, 10); editCount = std::clamp(editCount, 0, 999999);
                    if (ImGui::Button(T("确认修改", "Confirm"), ImVec2(S(190), S(38)))) { if (selected >= 0) counts[selected] = editCount; ImGui::CloseCurrentPopup(); }
                    ImGui::SameLine(); if (ImGui::Button(T("取消", "Cancel"), ImVec2(S(190), S(38)))) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            ImGui::End();
        }

        void RenderSpawnerWindow()
        {
            if (!g_State.SpawnerWindow) return;
            static int selected = 0, count = 1, location = 0, faction = 0;
            const char* entitiesZh[] = { "猛爪兽 1", "追猎者 1", "叛军骑手 1", "火焰滑翔者 1", "跃鞭兽 1", "雷霆牙 1" };
            const char* entitiesEn[] = { "Clawstrider 1", "Stalker 1", "Rebel Rider 1", "Fire Sunwing 1", "Leaplasher 1", "Thunderjaw 1" };
            const char* factionsZh[] = { "<未指定阵营>", "Player", "Neutral", "EnemyToAll" };
            const char* factionsEn[] = { "<No faction>", "Player", "Neutral", "EnemyToAll" };
            ImGui::SetNextWindowSize(ImVec2(S(900), S(650)), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(T("实体生成器###SpawnerPreview", "Entity spawner###SpawnerPreview"), &g_State.SpawnerWindow, ImGuiWindowFlags_NoCollapse))
            {
                const float left = ImGui::GetContentRegionAvail().x * .56f;
                ImGui::BeginChild("##entity-list", ImVec2(left, -FLT_MIN), true);
                ImGui::TextUnformatted(T("筛选（包含、-排除）", "Filter (include, -exclude)")); static char search[80] {}; ImGui::InputText("##entity-search", search, sizeof(search));
                for (int i = 0; i < IM_ARRAYSIZE(entitiesZh); ++i)
                    if (ImGui::Selectable(g_State.Chinese ? entitiesZh[i] : entitiesEn[i], selected == i)) selected = i;
                ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginChild("##spawn-settings", ImVec2(0, -FLT_MIN), true);
                ImGui::TextUnformatted(T("生成设置", "SPAWN SETTINGS")); ImGui::Separator();
                ImGui::TextUnformatted(T("生成数量", "Spawn count")); ImGui::InputInt("##spawn-count", &count); count = std::max(count, 1);
                Combo("faction", T("玩家阵营", "Faction"), &faction, g_State.Chinese ? factionsZh : factionsEn, IM_ARRAYSIZE(factionsZh));
                ImGui::TextUnformatted(T("生成位置", "Spawn position")); ImGui::RadioButton(T("玩家", "Player"), &location, 0); ImGui::SameLine(); ImGui::RadioButton(T("准星", "Crosshair"), &location, 1); ImGui::RadioButton(T("自定义", "Custom"), &location, 2);
                if (location == 2) { static float xyz[3] {}; ImGui::InputFloat3("XYZ", xyz); }
                ImGui::Spacing(); ImGui::Button(T("生成", "Spawn"), ImVec2(ImGui::GetContentRegionAvail().x, S(38)));
                ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), ImGui::GetWindowHeight() - S(85)));
                ImGui::TextColored(ImVec4(1, .75f, .3f, 1), "%s", T("警告：人形和脚本实体可能导致游戏崩溃。", "Warning: humanoid and scripted entities may crash the game."));
                ImGui::EndChild();
            }
            ImGui::End();
        }

        void RenderWeatherWindow()
        {
            if (!g_State.WeatherWindow) return;
            static int selected = 0;
            const char* weatherZh[] = { "晴朗海岸", "多云山地", "强降雨", "沙尘暴", "夜间薄雾" };
            const char* weatherEn[] = { "Clear Coast", "Cloudy Mountains", "Heavy Rain", "Sandstorm", "Night Fog" };
            ImGui::SetNextWindowSize(ImVec2(S(720), S(560)), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(T("天气设置###WeatherPreview", "Weather setup###WeatherPreview"), &g_State.WeatherWindow, ImGuiWindowFlags_NoCollapse))
            {
                static char filter[80] {}; ImGui::InputTextWithHint("##weather-filter", T("筛选（包含、-排除）", "Filter (include, -exclude)"), filter, sizeof(filter));
                Toggle("weather-ids", T("显示内部资源 ID（高级）", "Show internal resource IDs"), &g_State.ResourceIds);
                if (ImGui::BeginListBox("##weather-list", ImVec2(-FLT_MIN, ImGui::GetContentRegionAvail().y - S(80))))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(weatherZh); ++i) if (ImGui::Selectable(g_State.Chinese ? weatherZh[i] : weatherEn[i], selected == i)) selected = i;
                    ImGui::EndListBox();
                }
                ImGui::Button(T("应用天气", "Apply weather"), ImVec2(S(240), S(36)));
                ImGui::TextColored(ImVec4(1, .75f, .3f, 1), "%s", T("注意：部分名称缺失，可以在配置文件中补充。", "Note: missing names may be supplied in the configuration file."));
            }
            ImGui::End();
        }

        void RenderLocationsWindow()
        {
            if (!g_State.LocationsWindow) return;
            const char* const locationsZh[] = {
                "HZD - 子午城入口", "HZD - 尖塔", "HZD - 炼铸厂 ZETA", "HFW - 序章教学区域",
                "HFW - 贫瘠之光山地要塞", "HFW - 炙炎海岸首领战区域", "HFW - 法尔·泽尼斯基地"
            };
            const char* const locationsEn[] = {
                "HZD - Meridian Entrance", "HZD - The Spire", "HZD - Cauldron ZETA", "HFW - Prologue Tutorial",
                "HFW - Barren Light Fortress", "HFW - Burning Shores Boss Area", "HFW - Far Zenith Base"
            };
            ImGui::SetNextWindowSize(ImVec2(S(720), S(560)), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(T("预设地点###LocationsPreview", "Preset locations###LocationsPreview"), &g_State.LocationsWindow, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(Theme::Muted, "%s", T("选择地点后执行模拟传送。", "Select a location to run a mock teleport."));
                for (int i = 0; i < IM_ARRAYSIZE(locationsZh); ++i)
                {
                    ImGui::PushID(i);
                    ActionRow("##location", g_State.Chinese ? locationsZh[i] : locationsEn[i], T("传送", "Teleport"));
                    ImGui::PopID();
                }
            }
            ImGui::End();
        }

        void RenderDeveloperWindows()
        {
            if (g_State.LogWindow)
            {
                ImGui::SetNextWindowSize(ImVec2(S(760), S(440)), ImGuiCond_FirstUseEver);
                if (ImGui::Begin(T("模组日志###LogPreview", "Mod log###LogPreview"), &g_State.LogWindow))
                {
                    ImGui::TextColored(Theme::Muted, "[info] Initializing HFW Gameplay Tweaks");
                    ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "[info] Trainer signatures initialized");
                    ImGui::TextColored(Theme::Muted, "[info] DirectX 12 overlay ready");
                }
                ImGui::End();
            }
            if (g_State.DemoWindow) ImGui::ShowDemoWindow(&g_State.DemoWindow);
        }

        const char* PageName()
        {
            switch (g_Page)
            {
            case Page::Player: return T("玩家功能", "Player features");
            case Page::Resources: return T("资源与成长", "Resources & progression");
            case Page::World: return T("世界与传送", "World & teleport");
            case Page::Developer: return T("开发者工具", "Developer tools");
            case Page::Settings: return T("界面设置", "Interface settings");
            }
            return "HFW";
        }

        void Backdrop(ImVec2 size)
        {
            ImDrawList* d = ImGui::GetBackgroundDrawList();
            d->AddRectFilledMultiColor(ImVec2(0, 0), size, IM_COL32(15, 20, 31, 255), IM_COL32(23, 28, 43, 255), IM_COL32(7, 12, 21, 255), IM_COL32(10, 16, 27, 255));
            d->AddCircleFilled(ImVec2(size.x * .18f, size.y * .32f), size.y * .29f, IM_COL32(36, 63, 86, 48), 64);
            d->AddCircleFilled(ImVec2(size.x * .84f, size.y * .70f), size.y * .34f, IM_COL32(72, 48, 26, 40), 64);
        }
    }

    void ApplyStyle(float scale)
    {
        g_DpiScale = std::clamp(scale, .9f, 1.75f);
        g_Scale = g_DpiScale;
        ImGui::StyleColorsDark();
        ImGuiStyle& st = ImGui::GetStyle();
        st.WindowPadding = ImVec2(S(10), S(10)); st.FramePadding = ImVec2(S(10), S(6)); st.ItemSpacing = ImVec2(S(9), S(9)); st.ItemInnerSpacing = ImVec2(S(7), S(5));
        st.ScrollbarSize = S(12); st.GrabMinSize = S(11); st.WindowRounding = S(8); st.ChildRounding = S(6); st.PopupRounding = S(6); st.FrameRounding = S(5); st.ScrollbarRounding = S(7);
        st.WindowBorderSize = 1; st.ChildBorderSize = 0; st.FrameBorderSize = 0;
        auto& c = st.Colors;
        c[ImGuiCol_Text] = Theme::Text; c[ImGuiCol_TextDisabled] = Theme::Muted; c[ImGuiCol_WindowBg] = Theme::Window; c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_PopupBg] = Theme::Window;
        c[ImGuiCol_Border] = Theme::Border; c[ImGuiCol_FrameBg] = Theme::FrameOff; c[ImGuiCol_FrameBgHovered] = Theme::FrameOn; c[ImGuiCol_FrameBgActive] = Theme::FrameOn;
        c[ImGuiCol_Button] = Theme::Button; c[ImGuiCol_ButtonHovered] = Theme::ButtonHover; c[ImGuiCol_ButtonActive] = Theme::FrameOn; c[ImGuiCol_Header] = ImVec4(1, 1, 1, .03f); c[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, .06f); c[ImGuiCol_HeaderActive] = ImVec4(1, 1, 1, .09f); c[ImGuiCol_Separator] = Theme::Border; c[ImGuiCol_NavHighlight] = Theme::Accent;
    }

    void LoadFonts(float scale)
    {
        ImGuiIO& io = ImGui::GetIO();
        const float s = std::clamp(scale, .9f, 1.6f);
        ImFontConfig cfg {}; cfg.PixelSnapH = true; cfg.OversampleH = 3; cfg.OversampleV = 2; cfg.RasterizerMultiply = 1.18f;
        const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
        const float regularSize = std::round(17.5f * s);
        const float boldSize = std::round(23.0f * s);
        for (const auto& p : std::array<std::filesystem::path, 3>{ L"C:/Windows/Fonts/msyh.ttc", L"C:/Windows/Fonts/segoeui.ttf", L"C:/Windows/Fonts/simhei.ttf" })
        {
            std::error_code e; if (std::filesystem::is_regular_file(p, e)) { g_Regular = io.Fonts->AddFontFromFileTTF(p.string().c_str(), regularSize, &cfg, ranges); if (g_Regular) break; }
        }
        for (const auto& p : std::array<std::filesystem::path, 2>{ L"C:/Windows/Fonts/msyhbd.ttc", L"C:/Windows/Fonts/seguisb.ttf" })
        {
            std::error_code e; if (std::filesystem::is_regular_file(p, e)) { g_Bold = io.Fonts->AddFontFromFileTTF(p.string().c_str(), boldSize, &cfg, ranges); if (g_Bold) break; }
        }
        if (!g_Regular) { cfg.SizePixels = regularSize; g_Regular = io.Fonts->AddFontDefault(&cfg); }
        if (!g_Bold) g_Bold = g_Regular;
        io.FontDefault = g_Regular;
    }

    void RenderMenu(bool& visible, bool& requestExit)
    {
        ImGuiIO& io = ImGui::GetIO();
        g_Scale = g_DpiScale;
        Backdrop(io.DisplaySize);
        if (!visible)
        {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, S(22)), ImGuiCond_Always, ImVec2(.5f, 0));
            ImGui::SetNextWindowBgAlpha(.82f);
            ImGui::Begin("##hidden", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(g_State.Chinese ? "按 INS 显示菜单" : "Press INS to show menu"); ImGui::End(); return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) requestExit = true;

        const float fit = std::min({ g_DpiScale, (io.DisplaySize.x - 36) / 1080.0f, (io.DisplaySize.y - 36) / 740.0f });
        g_Scale = fit;
        const ImVec2 menuSize(1080 * fit, 740 * fit);
        const float sidebar = 245 * fit, top = 70 * fit;
        ImGui::SetNextWindowSize(menuSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(io.DisplaySize * .5f, ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        if (ImGui::Begin("HFW Tools###MainPreviewWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            ImGuiWindow* w = ImGui::GetCurrentWindow(); ImDrawList* d = w->DrawList; const ImVec2 p = w->Pos, z = w->Size;
            for (int i = 8; i > 0; --i) d->AddRect(p - ImVec2((float)i, (float)i), p + z + ImVec2((float)i, (float)i), IM_COL32(0, 0, 0, 6), S(9));
            d->AddRectFilled(p, p + z, Col(Theme::Window), S(8)); d->AddRectFilled(p, p + ImVec2(sidebar, z.y), Col(Theme::Sidebar), S(8), ImDrawFlags_RoundCornersLeft); d->AddRect(p, p + z, Col(Theme::Border), S(8));
            d->AddLine(p + ImVec2(sidebar, 0), p + ImVec2(sidebar, z.y), Col(Theme::Border)); d->AddLine(p + ImVec2(sidebar, top), p + ImVec2(z.x, top), Col(Theme::Border));
            d->AddRectFilled(p + ImVec2(S(18), S(16)), p + ImVec2(S(54), S(52)), Col(Theme::FrameOn), S(7)); d->AddRect(p + ImVec2(S(18), S(16)), p + ImVec2(S(54), S(52)), Col(Theme::Accent, .25f), S(7));
            if (g_Bold) d->AddText(g_Bold, g_Bold->FontSize, p + ImVec2(S(24), S(20)), Col(Theme::Accent), "HF");
            d->AddText(p + ImVec2(S(66), S(18)), Col(Theme::Text), "HFW TOOLS"); d->AddText(p + ImVec2(S(66), S(38)), Col(Theme::Muted, .65f), g_State.Chinese ? "界面预览" : "Interface preview");

            ImGui::SetCursorPos(ImVec2(S(14), S(78))); ImGui::BeginChild("##nav", ImVec2(sidebar - S(28), z.y - S(148)), false, ImGuiWindowFlags_NoScrollbar);
            Category(T("修改器", "TRAINER"));
            Nav("player", Icon::User, T("玩家功能", "Player features"), Page::Player, g_State.InGame);
            Nav("resources", Icon::Bag, T("资源与成长", "Resources & progression"), Page::Resources, g_State.InGame);
            Nav("world", Icon::Globe, T("世界与传送", "World & teleport"), Page::World);
            ImGui::Dummy(ImVec2(0, S(9))); Category(T("系统", "SYSTEM"));
            Nav("developer", Icon::Move, T("开发者工具", "Developer tools"), Page::Developer);
            Nav("settings", Icon::Gear, T("界面设置", "Interface settings"), Page::Settings); ImGui::EndChild();

            d->AddLine(p + ImVec2(0, z.y - S(66)), p + ImVec2(sidebar, z.y - S(66)), Col(Theme::Border)); d->AddCircleFilled(p + ImVec2(S(35), z.y - S(33)), S(17), Col(Theme::FrameOn), 24); IconGlyph(d, Icon::User, p + ImVec2(S(35), z.y - S(33)), Col(Theme::Accent), .78f);
            d->AddText(p + ImVec2(S(60), z.y - S(46)), Col(Theme::Text), g_State.Chinese ? "本地预览" : "Local preview"); d->AddText(p + ImVec2(S(60), z.y - S(26)), Col(Theme::Muted, .72f), g_State.Chinese ? "DX12 / 模拟状态" : "DX12 / Mock state");

            ImGui::SetCursorPos(ImVec2(sidebar + S(22), S(17))); Icon save = Icon::Save; FlatButton("save", g_State.Chinese ? "保存" : "Save", ImVec2(S(92), S(31)), &save);
            if (g_Page == Page::Player)
            {
                ImGui::SetCursorPos(ImVec2(sidebar + S(130), S(17))); const char* zh[] = { "状态", "战斗", "移动", "其他" }; const char* en[] = { "Status", "Combat", "Move", "Other" };
                for (int i = 0; i < 4; ++i)
                {
                    ImGui::PushID(i); ImDrawFlags flags = i == 0 ? ImDrawFlags_RoundCornersLeft : (i == 3 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_None);
                    if (Subtab("##subtab", g_State.Chinese ? zh[i] : en[i], g_Subtab == i, ImVec2(S(76), S(31)), flags) && g_Subtab != i) { g_Subtab = i; g_PageAnim = 0; }
                    ImGui::PopID(); if (i != 3) ImGui::SameLine(0, 0);
                }
            }
            ImGui::SetCursorPos(ImVec2(z.x - S(318), S(17))); if (FlatButton("session", g_State.InGame ? (g_State.Chinese ? "游戏内" : "In game") : (g_State.Chinese ? "大厅" : "Lobby"), ImVec2(S(126), S(31)))) g_State.InGame = !g_State.InGame;
            ImGui::SetCursorPos(ImVec2(z.x - S(176), S(17))); if (FlatButton("lang", g_State.Chinese ? "中 / EN" : "EN / 中", ImVec2(S(92), S(31)))) g_State.Chinese = !g_State.Chinese; ImGui::SameLine(0, S(8)); if (FlatButton("close", "X", ImVec2(S(34), S(31)))) requestExit = true;
            if (g_Bold) d->AddText(g_Bold, g_Regular ? g_Regular->FontSize : ImGui::GetFontSize(), p + ImVec2(sidebar + S(22), top + S(15)), Col(Theme::Muted, .78f), PageName());
            else d->AddText(p + ImVec2(sidebar + S(22), top + S(16)), Col(Theme::Muted, .65f), PageName());

            g_PageAnim = Ease(g_PageAnim, 1, 9.5f); const float contentTop = top + S(42); const ImVec2 cp(sidebar + S(22), contentTop + S(7) * (1 - g_PageAnim)); const ImVec2 cs(z.x - sidebar - S(44), z.y - contentTop - S(20));
            ImGui::SetCursorPos(cp); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(g_PageAnim, .02f, 1.0f)); ImGui::BeginChild("##content", cs, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const bool unavailable = PageNeedsPlayer(g_Page) && !g_State.InGame;
            float noticeHeight = unavailable ? LobbyNotice() : 0.0f;
            ImVec2 pageSize(cs.x, cs.y - noticeHeight);
            if (unavailable) ImGui::BeginDisabled();
            if (g_Page == Page::Player) PlayerPage(pageSize); else StandardPage(pageSize);
            if (unavailable) ImGui::EndDisabled();
            ImGui::EndChild(); ImGui::PopStyleVar();
            RenderValueEditor();
            RenderInventoryWindow();
            RenderSpawnerWindow();
            RenderWeatherWindow();
            RenderLocationsWindow();
            RenderDeveloperWindows();
        }
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar();
    }
}
