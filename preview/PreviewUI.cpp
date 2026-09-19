#include "PreviewUI.h"

#include <algorithm>
#include <array>
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

        enum class Page { Player, Movement, Resources, World, Teleport, Settings };
        enum class Icon { User, Move, Bag, Globe, Pin, Gear, Save };

        struct State
        {
            bool Chinese = true;
            bool InGame = false;
            bool Invincible = false;
            bool InfiniteHealth = false;
            bool IgnoreDamage = false;
            bool InfiniteStamina = false;
            bool NoFallDamage = false;
            bool InstantCharge = false;
            bool InfiniteAmmo = false;
            bool InfiniteArrows = false;
            bool NoReload = false;
            bool MountAnywhere = false;
            bool AiVsAi = false;
            bool InfiniteJump = false;
            bool Noclip = false;
            bool FreezeTime = false;
            bool WeatherOverride = false;
            bool RevealMap = false;
            bool FreeCrafting = false;
            bool ResourceIds = false;
            bool HardwareCursor = false;
            bool DamageMultiplier = false;
            bool DefenseMultiplier = false;
            bool MovementSpeed = false;
            bool FallSpeed = false;
            float Damage = 2.0f;
            float Defense = 2.0f;
            float Speed = 3.0f;
            float Jump = 5.0f;
            float Falling = 0.5f;
            float Time = 12.0f;
            float Amount = 999.0f;
            float InterfaceScale = 1.0f;
            int Weather = 0;
            int Location = 0;
        };

        State g_State;
        Page g_Page = Page::Player;
        int g_Subtab = 0;
        float g_PageAnim = 1.0f;
        float g_Scale = 1.0f;
        ImFont* g_Regular = nullptr;
        ImFont* g_Bold = nullptr;
        std::unordered_map<ImGuiID, float> g_Anim;

        float S(float value) { return value * g_Scale; }

        bool PageNeedsPlayer(Page page)
        {
            return page == Page::Player || page == Page::Movement || page == Page::Resources || page == Page::Teleport;
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
            const ImVec2 ts = ImGui::CalcTextSize(label);
            ImVec4 textColor = Mix(Theme::Muted, Theme::Text, lit);
            if (!enabled) textColor.w = .38f;
            w->DrawList->AddText(bb.Min + ImVec2(S(40), (bb.GetHeight() - ts.y) * .5f), Col(textColor), label);
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
            const ImVec2 ts = ImGui::CalcTextSize(text);
            w->DrawList->AddText(bb.GetCenter() - ts * .5f, Col(Mix(Theme::Muted, Theme::Text, std::max(a, h * .7f))), text);
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
            d->AddText(p + ImVec2(S(13), 0), Col(Theme::Muted, .78f), title);
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
                        BeginBox("survival", g_State.Chinese ? "生存状态" : "SURVIVAL", ImVec2(w, S(245)));
                        Toggle("invincible", g_State.Chinese ? "无敌模式" : "Invincible", &g_State.Invincible);
                        Toggle("health", g_State.Chinese ? "无限生命" : "Infinite health", &g_State.InfiniteHealth);
                        Toggle("damage", g_State.Chinese ? "无视伤害判定" : "Ignore damage checks", &g_State.IgnoreDamage);
                        Toggle("stamina", g_State.Chinese ? "无限体力" : "Infinite stamina", &g_State.InfiniteStamina);
                        Toggle("fall", g_State.Chinese ? "免疫坠落伤害" : "No fall damage", &g_State.NoFallDamage);
                        EndBox();
                        BeginBox("actions", g_State.Chinese ? "角色动作" : "PLAYER ACTIONS", ImVec2(w, size.y - S(255)));
                        Toggle("charge", g_State.Chinese ? "弓箭快速蓄力" : "Fast bow charge", &g_State.InstantCharge);
                        Toggle("mount", g_State.Chinese ? "解除坐骑限制" : "Mount anywhere", &g_State.MountAnywhere);
                        Toggle("aivsai", g_State.Chinese ? "允许 AI 互相伤害" : "Enable AI vs AI damage", &g_State.AiVsAi);
                        EndBox();
                    }
                    else
                    {
                        BeginBox("player-advanced", g_Subtab == 1 ? (g_State.Chinese ? "高级数值" : "ADVANCED VALUES") : (g_State.Chinese ? "快捷键" : "HOTKEYS"), ImVec2(w, size.y));
                        if (g_Subtab == 1)
                        {
                            Toggle("damage-mult-enable", g_State.Chinese ? "启用伤害倍率" : "Enable damage multiplier", &g_State.DamageMultiplier);
                            ImGui::BeginDisabled(!g_State.DamageMultiplier);
                            Slider("damage-mult", g_State.Chinese ? "伤害倍率数值" : "Damage value", &g_State.Damage, 1, 10, "%.1fx");
                            ImGui::EndDisabled();
                            Line();
                            Toggle("defense-mult-enable", g_State.Chinese ? "启用防御倍率" : "Enable defense multiplier", &g_State.DefenseMultiplier);
                            ImGui::BeginDisabled(!g_State.DefenseMultiplier);
                            Slider("defense-mult", g_State.Chinese ? "防御倍率数值" : "Defense value", &g_State.Defense, 1, 10, "%.1fx");
                            ImGui::EndDisabled();
                        }
                        else
                        {
                            ImGui::TextColored(Theme::Muted, "INS"); ImGui::SameLine(S(100)); ImGui::TextUnformatted(g_State.Chinese ? "显示 / 隐藏菜单" : "Show / hide menu"); Line();
                            ImGui::TextColored(Theme::Muted, "` / ~"); ImGui::SameLine(S(100)); ImGui::TextUnformatted(g_State.Chinese ? "穿墙模式" : "Noclip mode"); Line();
                            ImGui::TextColored(Theme::Muted, "SPACE"); ImGui::SameLine(S(100)); ImGui::TextUnformatted(g_State.Chinese ? "踏空跳 / 上升" : "Air jump / ascend");
                        }
                        EndBox();
                    }
                },
                [&](float w)
                {
                    BeginBox("weapons", g_Subtab == 0 ? (g_State.Chinese ? "武器与弹药" : "WEAPONS & AMMO") : (g_State.Chinese ? "状态与说明" : "STATUS & NOTES"), ImVec2(w, size.y));
                    if (g_Subtab == 0)
                    {
                        Toggle("ammo", g_State.Chinese ? "无限弹药" : "Infinite ammo", &g_State.InfiniteAmmo);
                        Toggle("arrows", g_State.Chinese ? "无限箭矢与陷阱" : "Infinite arrows and traps", &g_State.InfiniteArrows);
                        Toggle("reload", g_State.Chinese ? "无需装填" : "No reload", &g_State.NoReload);
                    }
                    else if (g_Subtab == 1)
                    {
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", g_State.Chinese ? "DLL 已动态加载" : "DLL loaded dynamically");
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", g_State.Chinese ? "DirectX 12 渲染正常" : "DirectX 12 renderer ready");
                        ImGui::TextColored(Theme::Muted, "%s", g_State.Chinese ? "数值修改默认关闭" : "Value modifiers default to off");
                        ImGui::TextColored(Theme::Muted, "FPS  %.0f", ImGui::GetIO().Framerate);
                    }
                    else
                    {
                        ImGui::TextColored(Theme::Muted, "%s", g_State.Chinese ? "预览程序没有游戏功能。" : "The preview has no game functions.");
                        ImGui::TextWrapped(g_State.Chinese ? "所有控件只使用本地模拟状态，用于检查视觉效果和动画。" : "All controls use local mock state for visual and animation testing.");
                    }
                    EndBox();
                });
        }

        void StandardPage(ImVec2 size)
        {
            TwoColumns(size,
                [&](float w)
                {
                    const char* title = g_State.Chinese ? "主要设置" : "MAIN";
                    BeginBox("main", title, ImVec2(w, size.y));
                    if (g_Page == Page::Movement)
                    {
                        Toggle("move-enable", g_State.Chinese ? "启用移动速度调整" : "Enable movement speed", &g_State.MovementSpeed);
                        ImGui::BeginDisabled(!g_State.MovementSpeed);
                        Slider("move", g_State.Chinese ? "移动速度倍率" : "Movement multiplier", &g_State.Speed, 1, 8, "%.1fx");
                        ImGui::EndDisabled();
                        Toggle("infinite-jump", g_State.Chinese ? "启用无限踏空跳" : "Enable infinite air jump", &g_State.InfiniteJump);
                        ImGui::BeginDisabled(!g_State.InfiniteJump);
                        Slider("jump", g_State.Chinese ? "跳跃高度倍率" : "Jump height multiplier", &g_State.Jump, 1, 20, "%.1f");
                        ImGui::EndDisabled();
                        Toggle("fall-enable", g_State.Chinese ? "启用下降速度调整" : "Enable falling speed", &g_State.FallSpeed);
                        ImGui::BeginDisabled(!g_State.FallSpeed);
                        Slider("fall", g_State.Chinese ? "下降速度倍率" : "Falling multiplier", &g_State.Falling, .1f, 3, "%.1fx");
                        ImGui::EndDisabled();
                        Toggle("noclip-main", g_State.Chinese ? "自由飞行穿墙" : "Free-flight noclip", &g_State.Noclip);
                    }
                    else if (g_Page == Page::Resources)
                    {
                        Slider("amount", g_State.Chinese ? "目标数量" : "Target amount", &g_State.Amount, 0, 9999, "%.0f");
                        Toggle("craft", g_State.Chinese ? "无视制作与购买需求" : "Ignore crafting and purchase costs", &g_State.FreeCrafting);
                        Toggle("ids", g_State.Chinese ? "显示内部资源 ID" : "Show internal resource IDs", &g_State.ResourceIds);
                        FlatButton("apply", g_State.Chinese ? "应用模拟数值" : "Apply mock value", ImVec2(ImGui::GetContentRegionAvail().x, S(31)));
                    }
                    else if (g_Page == Page::World)
                    {
                        Toggle("freeze", g_State.Chinese ? "锁定世界时间" : "Freeze world time", &g_State.FreezeTime);
                        Slider("time", g_State.Chinese ? "当前时间" : "Current time", &g_State.Time, 0, 24, "%.1fh");
                        Toggle("weather", g_State.Chinese ? "覆盖当前天气" : "Override current weather", &g_State.WeatherOverride);
                        Toggle("map", g_State.Chinese ? "显示完整地图" : "Reveal full map", &g_State.RevealMap);
                    }
                    else if (g_Page == Page::Teleport)
                    {
                        const char* placesZh[] = { "基地", "炙矛地", "削链镇", "竞技场", "旧金山遗迹" };
                        const char* placesEn[] = { "The Base", "Scalding Spear", "Chainscrape", "The Arena", "San Francisco Ruins" };
                        const char* const* places = g_State.Chinese ? placesZh : placesEn;
                        Combo("location", g_State.Chinese ? "目标位置" : "Destination", &g_State.Location, places, IM_ARRAYSIZE(placesZh));
                        FlatButton("teleport", g_State.Chinese ? "执行模拟传送" : "Run mock teleport", ImVec2(ImGui::GetContentRegionAvail().x, S(31)));
                    }
                    else
                    {
                        Toggle("hardware", g_State.Chinese ? "硬件鼠标光标" : "Hardware mouse cursor", &g_State.HardwareCursor);
                        Slider("ui", g_State.Chinese ? "界面缩放" : "Interface scale", &g_State.InterfaceScale, .85f, 1.5f, "%.2fx");
                    }
                    EndBox();
                },
                [&](float w)
                {
                    BeginBox("secondary", g_State.Chinese ? "状态与预览" : "STATUS & PREVIEW", ImVec2(w, size.y));
                    if (g_Page == Page::World)
                    {
                        const char* weatherZh[] = { "晴朗", "多云", "雨天", "沙尘暴" };
                        const char* weatherEn[] = { "Clear", "Cloudy", "Rain", "Sandstorm" };
                        const char* const* weather = g_State.Chinese ? weatherZh : weatherEn;
                        Combo("weather-select", g_State.Chinese ? "天气" : "Weather", &g_State.Weather, weather, IM_ARRAYSIZE(weatherZh));
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(.38f, .78f, .55f, 1), "%s", g_State.Chinese ? "DLL 已加载" : "DLL loaded");
                        ImGui::TextColored(Theme::Muted, "%s", g_State.Chinese ? "仅使用本地模拟状态" : "Local mock state only");
                        Line();
                        ImGui::TextWrapped(g_State.Chinese ? "本页用于检查控件比例、字体、弹层和动画，不访问游戏内存。" : "This page tests control proportions, typography, popups and animation without game memory access.");
                    }
                    EndBox();
                });
        }

        const char* PageName()
        {
            switch (g_Page)
            {
            case Page::Player: return g_State.Chinese ? "玩家" : "Player";
            case Page::Movement: return g_State.Chinese ? "移动" : "Movement";
            case Page::Resources: return g_State.Chinese ? "资源" : "Resources";
            case Page::World: return g_State.Chinese ? "世界" : "World";
            case Page::Teleport: return g_State.Chinese ? "传送" : "Teleport";
            case Page::Settings: return g_State.Chinese ? "设置" : "Settings";
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
        g_Scale = std::clamp(scale, .9f, 1.6f);
        ImGui::StyleColorsDark();
        ImGuiStyle& st = ImGui::GetStyle();
        st.WindowPadding = ImVec2(S(8), S(8)); st.FramePadding = ImVec2(S(8), S(5)); st.ItemSpacing = ImVec2(S(8), S(8)); st.ItemInnerSpacing = ImVec2(S(6), S(4));
        st.ScrollbarSize = S(10); st.GrabMinSize = S(9); st.WindowRounding = S(7); st.ChildRounding = S(5); st.PopupRounding = S(5); st.FrameRounding = S(4); st.ScrollbarRounding = S(6);
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
        ImFontConfig cfg {}; cfg.PixelSnapH = true; cfg.OversampleH = 2; cfg.OversampleV = 2; cfg.RasterizerMultiply = 1.1f;
        const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
        for (const auto& p : std::array<std::filesystem::path, 3>{ L"C:/Windows/Fonts/msyh.ttc", L"C:/Windows/Fonts/segoeui.ttf", L"C:/Windows/Fonts/simhei.ttf" })
        {
            std::error_code e; if (std::filesystem::is_regular_file(p, e)) { g_Regular = io.Fonts->AddFontFromFileTTF(p.string().c_str(), 15.5f * s, &cfg, ranges); if (g_Regular) break; }
        }
        for (const auto& p : std::array<std::filesystem::path, 2>{ L"C:/Windows/Fonts/msyhbd.ttc", L"C:/Windows/Fonts/seguisb.ttf" })
        {
            std::error_code e; if (std::filesystem::is_regular_file(p, e)) { g_Bold = io.Fonts->AddFontFromFileTTF(p.string().c_str(), 21.0f * s, &cfg, ranges); if (g_Bold) break; }
        }
        if (!g_Regular) { cfg.SizePixels = 15.5f * s; g_Regular = io.Fonts->AddFontDefault(&cfg); }
        if (!g_Bold) g_Bold = g_Regular;
        io.FontDefault = g_Regular;
    }

    void RenderMenu(bool& visible, bool& requestExit)
    {
        ImGuiIO& io = ImGui::GetIO();
        Backdrop(io.DisplaySize);
        if (!visible)
        {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, S(22)), ImGuiCond_Always, ImVec2(.5f, 0));
            ImGui::SetNextWindowBgAlpha(.82f);
            ImGui::Begin("##hidden", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(g_State.Chinese ? "按 INS 显示菜单" : "Press INS to show menu"); ImGui::End(); return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) requestExit = true;

        const float fit = std::min({ g_Scale, (io.DisplaySize.x - 36) / 940.0f, (io.DisplaySize.y - 36) / 660.0f });
        const ImVec2 menuSize(940 * fit, 660 * fit);
        const float sidebar = 220 * fit, top = 64 * fit;
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
            Category(g_State.Chinese ? "角色" : "PLAYER"); Nav("player", Icon::User, g_State.Chinese ? "玩家" : "Player", Page::Player, g_State.InGame); Nav("move", Icon::Move, g_State.Chinese ? "移动与镜头" : "Movement", Page::Movement, g_State.InGame);
            ImGui::Dummy(ImVec2(0, S(9))); Category(g_State.Chinese ? "游戏" : "GAME"); Nav("resources", Icon::Bag, g_State.Chinese ? "资源与物品" : "Resources", Page::Resources, g_State.InGame); Nav("world", Icon::Globe, g_State.Chinese ? "世界" : "World", Page::World); Nav("teleport", Icon::Pin, g_State.Chinese ? "传送" : "Teleport", Page::Teleport, g_State.InGame);
            ImGui::Dummy(ImVec2(0, S(9))); Category(g_State.Chinese ? "通用" : "COMMON"); Nav("settings", Icon::Gear, g_State.Chinese ? "设置" : "Settings", Page::Settings); ImGui::EndChild();

            d->AddLine(p + ImVec2(0, z.y - S(66)), p + ImVec2(sidebar, z.y - S(66)), Col(Theme::Border)); d->AddCircleFilled(p + ImVec2(S(35), z.y - S(33)), S(17), Col(Theme::FrameOn), 24); IconGlyph(d, Icon::User, p + ImVec2(S(35), z.y - S(33)), Col(Theme::Accent), .78f);
            d->AddText(p + ImVec2(S(60), z.y - S(46)), Col(Theme::Text), g_State.Chinese ? "本地预览" : "Local preview"); d->AddText(p + ImVec2(S(60), z.y - S(26)), Col(Theme::Muted, .72f), g_State.Chinese ? "DX12 / 模拟状态" : "DX12 / Mock state");

            ImGui::SetCursorPos(ImVec2(sidebar + S(22), S(17))); Icon save = Icon::Save; FlatButton("save", g_State.Chinese ? "保存" : "Save", ImVec2(S(92), S(31)), &save);
            if (g_Page == Page::Player)
            {
                ImGui::SetCursorPos(ImVec2(sidebar + S(130), S(17))); const char* zh[] = { "常用", "高级", "快捷键" }; const char* en[] = { "General", "Advanced", "Hotkeys" };
                for (int i = 0; i < 3; ++i)
                {
                    ImGui::PushID(i); ImDrawFlags flags = i == 0 ? ImDrawFlags_RoundCornersLeft : (i == 2 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_None);
                    if (Subtab("##subtab", g_State.Chinese ? zh[i] : en[i], g_Subtab == i, ImVec2(S(90), S(31)), flags) && g_Subtab != i) { g_Subtab = i; g_PageAnim = 0; }
                    ImGui::PopID(); if (i != 2) ImGui::SameLine(0, 0);
                }
            }
            ImGui::SetCursorPos(ImVec2(z.x - S(318), S(17))); if (FlatButton("session", g_State.InGame ? (g_State.Chinese ? "游戏内" : "In game") : (g_State.Chinese ? "大厅" : "Lobby"), ImVec2(S(126), S(31)))) g_State.InGame = !g_State.InGame;
            ImGui::SetCursorPos(ImVec2(z.x - S(176), S(17))); if (FlatButton("lang", g_State.Chinese ? "中 / EN" : "EN / 中", ImVec2(S(92), S(31)))) g_State.Chinese = !g_State.Chinese; ImGui::SameLine(0, S(8)); if (FlatButton("close", "X", ImVec2(S(34), S(31)))) requestExit = true;
            d->AddText(p + ImVec2(sidebar + S(22), top + S(16)), Col(Theme::Muted, .65f), PageName());

            g_PageAnim = Ease(g_PageAnim, 1, 9.5f); const float contentTop = top + S(42); const ImVec2 cp(sidebar + S(22), contentTop + S(7) * (1 - g_PageAnim)); const ImVec2 cs(z.x - sidebar - S(44), z.y - contentTop - S(20));
            ImGui::SetCursorPos(cp); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(g_PageAnim, .02f, 1.0f)); ImGui::BeginChild("##content", cs, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const bool unavailable = PageNeedsPlayer(g_Page) && !g_State.InGame;
            float noticeHeight = unavailable ? LobbyNotice() : 0.0f;
            ImVec2 pageSize(cs.x, cs.y - noticeHeight);
            if (unavailable) ImGui::BeginDisabled();
            if (g_Page == Page::Player) PlayerPage(pageSize); else StandardPage(pageSize);
            if (unavailable) ImGui::EndDisabled();
            ImGui::EndChild(); ImGui::PopStyleVar();
        }
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar();
    }
}
