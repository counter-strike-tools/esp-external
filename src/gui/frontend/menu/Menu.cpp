#include "Menu.hpp"
#include <algorithm>
#include <cstring>
#include <imgui_internal.h>
#include "assets/fonts/Icons.h"
#include "config/Config.hpp"
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"

namespace {
	struct Palette {
		ImVec4 bg = ImVec4(0.042f, 0.045f, 0.052f, 1.00f);
		ImVec4 side = ImVec4(0.052f, 0.056f, 0.064f, 1.00f);
		ImVec4 hover = ImVec4(0.075f, 0.082f, 0.094f, 1.00f);
		ImVec4 line = ImVec4(0.48f, 0.54f, 0.60f, 0.08f);
		ImVec4 text = ImVec4(0.91f, 0.93f, 0.95f, 1.00f);
		ImVec4 muted = ImVec4(0.55f, 0.60f, 0.66f, 1.00f);
		ImVec4 dim = ImVec4(0.35f, 0.39f, 0.45f, 1.00f);
		ImVec4 accent = ImVec4(0.36f, 0.58f, 0.92f, 1.00f);
		ImVec4 good = ImVec4(0.42f, 0.70f, 0.96f, 1.00f);
	};

	Palette ui;

	ImU32 Color(const ImVec4& color, float alpha = 1.0f)
	{
		ImVec4 c = color;
		c.w *= alpha * ImGui::GetStyle().Alpha;
		return ImGui::ColorConvertFloat4ToU32(c);
	}

	ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
	}

	float Anim(const char* id, bool active, float speed = 12.0f)
	{
		ImGuiStorage* storage = ImGui::GetStateStorage();
		ImGuiID key = ImGui::GetID(id);
		float value = storage->GetFloat(key, active ? 1.0f : 0.0f);
		value += ((active ? 1.0f : 0.0f) - value) * std::min(ImGui::GetIO().DeltaTime * speed, 1.0f);
		storage->SetFloat(key, value);
		return value;
	}

	bool NavItem(const TabItem& tab, bool active)
	{
		ImGui::PushID(tab.label.c_str());
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size(40.0f, 38.0f);
		ImGui::InvisibleButton("##nav", size);
		bool hovered = ImGui::IsItemHovered();
		float hot = Anim("hot", hovered || active, 14.0f);
		ImVec4 fill = Mix(ui.side, ui.hover, hot);

		auto* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(pos, pos + size, Color(fill, active ? 1.0f : 0.75f), 7.0f);
		if (active)
			draw->AddRectFilled(pos + ImVec2(12.0f, size.y - 2.0f), pos + ImVec2(size.x - 12.0f, size.y), Color(ui.accent), 2.0f);

		ImVec2 text_size = ImGui::CalcTextSize(tab.icon.c_str());
		draw->AddText(pos + ImVec2((size.x - text_size.x) * 0.5f, (size.y - text_size.y) * 0.5f - 1.0f), Color(active ? ui.accent : Mix(ui.muted, ui.text, hot)), tab.icon.c_str());
		ImGui::PopID();
		return ImGui::IsItemClicked();
	}

	bool ToggleRow(const char* id, const char* label, bool* value)
	{
		ImGui::PushID(id);
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size(ImGui::GetContentRegionAvail().x, 26.0f);
		ImGui::InvisibleButton("##row", size);
		bool hovered = ImGui::IsItemHovered();
		bool changed = false;
		if (ImGui::IsItemClicked()) {
			*value = !*value;
			changed = true;
		}

		float hot = Anim("hot", hovered, 16.0f);
		float on = Anim("on", *value, 16.0f);
		auto* draw = ImGui::GetWindowDrawList();
		if (hot > 0.01f)
			draw->AddRectFilled(pos, pos + size, Color(ui.hover, hot * 0.72f), 5.0f);

		draw->AddText(pos + ImVec2(2.0f, 4.0f), Color(Mix(ui.muted, ui.text, *value ? 1.0f : hot)), label);
		ImVec2 track = pos + ImVec2(size.x - 34.0f, 7.0f);
		draw->AddRectFilled(track, track + ImVec2(26.0f, 12.0f), Color(Mix(ui.dim, ui.accent, on), 0.70f), 6.0f);
		draw->AddCircleFilled(track + ImVec2(6.0f + on * 14.0f, 6.0f), 4.5f, Color(ui.text));
		ImGui::PopID();
		return changed;
	}

	void Section(const char* icon, const char* title)
	{
		(void)icon;
		ImGui::Dummy(ImVec2(0.0f, 7.0f));
		ImGui::TextColored(ui.muted, "%s", title);
	}

	void DetailsIn()
	{
		ImGui::Indent(10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));
	}

	void DetailsOut()
	{
		ImGui::PopStyleVar();
		ImGui::Unindent(10.0f);
	}

	void SliderFloatRow(const char* label, const char* id, float* value, float min, float max, const char* format)
	{
		ImGui::TextColored(ui.muted, "%s", label);
		ImGui::SameLine(118.0f);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::SliderFloat(id, value, min, max, format);
	}

	void SliderIntRow(const char* label, const char* id, int* value, int min, int max)
	{
		ImGui::TextColored(ui.muted, "%s", label);
		ImGui::SameLine(118.0f);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::SliderInt(id, value, min, max);
	}

	void SolidColorEdit(const char* label, float* color, ImGuiColorEditFlags flags)
	{
		color[3] = 1.0f;
		ImGui::ColorEdit4(label, color, flags | ImGuiColorEditFlags_NoAlpha);
		color[3] = 1.0f;
	}

	void ColorLabel(const char* label)
	{
		ImGui::TextColored(ui.muted, "%.*s", static_cast<int>(ImGui::FindRenderedTextEnd(label) - label), label);
	}

	void ColorPair(const char* left, float* left_color, const char* right, float* right_color, ImGuiColorEditFlags flags)
	{
		ImGui::PushID(left_color);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!ImGui::BeginCombo("##colors", "Colors")) {
			ImGui::PopID();
			return;
		}

		ColorLabel(left);
		ImGui::SameLine(92.0f);
		SolidColorEdit("##left", left_color, flags);
		ImGui::SameLine();
		ColorLabel(right);
		ImGui::SameLine();
		SolidColorEdit("##right", right_color, flags);
		ImGui::EndCombo();
		ImGui::PopID();
	}

	void ColorSingle(const char* label, float* color, ImGuiColorEditFlags flags)
	{
		ImGui::PushID(color);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!ImGui::BeginCombo("##colors", label)) {
			ImGui::PopID();
			return;
		}

		ColorLabel(label);
		ImGui::SameLine(92.0f);
		SolidColorEdit("##color", color, flags);
		ImGui::EndCombo();
		ImGui::PopID();
	}

	bool KeyCombo(const char* id, int* key)
	{
		const char* keys[] = { "Mouse 4", "Mouse 5", "Left Alt", "Left Shift" };
		int index = (*key == VK_XBUTTON1) ? 0 : (*key == VK_XBUTTON2) ? 1 : (*key == VK_MENU) ? 2 : 3;
		ImGui::TextColored(ui.muted, "Key");
		ImGui::SameLine(118.0f);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::Combo(id, &index, keys, IM_ARRAYSIZE(keys))) {
			*key = (index == 0) ? VK_XBUTTON1 : (index == 1) ? VK_XBUTTON2 : (index == 2) ? VK_MENU : VK_LSHIFT;
			return true;
		}
		return false;
	}

	const TabItem& CurrentTab(Tab id)
	{
		for (const auto& tab : tabs)
			if (tab.id == id)
				return tab;
		return tabs[0];
	}
}

bool Menu::Init() {
	return GetInstance().InitImpl();
}

void Menu::Render() {
	return GetInstance().RenderImpl();
}

void Menu::RenderStartupHelp() {
	return GetInstance().RenderStartupHelpImpl();
}

ImVec2 Menu::GetPos() {
	return GetInstance().pos;
}

ImVec2 Menu::GetSize() {
	return GetInstance().size;
}

bool Menu::InitImpl() {
	SetupStyles();
	theme::ThemeManager::Get().Init();

	LOGF(INFO, "Successfully initialized menu...");
	return true;
}

void Menu::RenderImpl() {
	if (!isSetup)
		return;

	static auto io = ImGui::GetIO();
	static auto screen = io.DisplaySize;
	static auto color_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha;
	static Tab active_tab = Tab::AIM;
	static bool pending_auto_save = false;
	static double last_edit_time = 0.0;

#ifdef _DEBUG
	static auto title = "cs2-external-danger [dev]";
#else
	static auto title = "cs2-external-danger";
#endif

	ImGui::SetNextWindowSize(ImVec2(760, 500), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(680, 430), ImVec2(980, 720));
	ImGui::SetNextWindowPos(ImVec2(screen.x / 2 - 380, screen.y / 2 - 250), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse)) {
		this->pos = ImGui::GetWindowPos();
		this->size = ImGui::GetWindowSize();

		ImGui::BeginChild("##nav", ImVec2(52.0f, 0.0f), ImGuiChildFlags_None);
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		for (const auto& tab : tabs) {
			if (NavItem(tab, active_tab == tab.id))
				active_tab = tab.id;
			ImGui::Dummy(ImVec2(0.0f, 4.0f));
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

		const TabItem& page = CurrentTab(active_tab);
		ImGui::TextColored(ui.accent, "%s", page.icon.c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(page.label.c_str());
		ImGui::PushStyleColor(ImGuiCol_Separator, ui.line);
		ImGui::Separator();
		ImGui::PopStyleColor();

		if (active_tab == Tab::AIM) {
			ImGui::Columns(2, "aim_columns", false);
			Section(Icons::PERSON, "Aim activation");
			ToggleRow("aimbot", "Aimbot", &cfg::aimbot::enabled);
			if (cfg::aimbot::enabled) {
				DetailsIn();
				ToggleRow("aim_always", "Always on", &cfg::aimbot::always_on);
				if (!cfg::aimbot::always_on)
					KeyCombo("##aim_key", &cfg::aimbot::hotkey);
				SliderFloatRow("FOV", "##aim_fov", &cfg::aimbot::fov, 1.0f, 40.0f, "%.1f");
				SliderFloatRow("Smooth", "##aim_smooth", &cfg::aimbot::smooth, 1.0f, 15.0f, "%.1f");
				DetailsOut();

				ImGui::NextColumn();
				Section(Icons::GLOBE, "Targeting");
				DetailsIn();
				ToggleRow("aim_visible", "Visible only", &cfg::aimbot::visible_only);
				ToggleRow("aim_multibone", "Multibone", &cfg::aimbot::multibone);
				if (cfg::aimbot::multibone) {
					DetailsIn();
					ToggleRow("aim_closest", "Closest bone", &cfg::aimbot::multibone_closest);
					ToggleRow("aim_interp", "Interpolate", &cfg::aimbot::multibone_interpolate);
					if (cfg::aimbot::multibone_interpolate)
						SliderIntRow("Steps", "##aim_interp_steps", &cfg::aimbot::multibone_interp_steps, 1, 5);
					ToggleRow("aim_exposed", "Exposed only", &cfg::aimbot::exposed_bones_only);
					DetailsOut();
				}
				ToggleRow("aim_velocity", "Velocity comp", &cfg::aimbot::velocity_comp);
				if (cfg::aimbot::velocity_comp)
					SliderFloatRow("Velocity", "##aim_velocity", &cfg::aimbot::velocity_comp_scale, 0.0f, 0.1f, "%.3f");
				ToggleRow("aim_rcs", "RCS link", &cfg::aimbot::rcs);
				DetailsOut();
			}

			ImGui::Columns(1);
			Section(Icons::BLIND, "Aim motion");
			ToggleRow("human", "Humanization", &cfg::aimbot::humanization);
			if (cfg::aimbot::humanization) {
				ImGui::Columns(2, "motion_columns", false);
				DetailsIn();
				ToggleRow("assist", "Aim assist", &cfg::aimbot::aim_assist);
				SliderFloatRow("Reaction", "##reaction", &cfg::aimbot::reaction_time_ms, 50.f, 500.f, "%.0f ms");
				SliderFloatRow("Error", "##aim_error", &cfg::aimbot::aim_error_px, 0.f, 15.f, "%.1f px");
				SliderFloatRow("Jitter", "##jitter", &cfg::aimbot::tracking_jitter, 0.f, 3.f, "%.1f");
				DetailsOut();
				ImGui::NextColumn();
				DetailsIn();
				SliderFloatRow("Miss", "##miss", &cfg::aimbot::miss_chance, 0.f, 0.5f, "%.2f");
				SliderFloatRow("Overshoot", "##overshoot", &cfg::aimbot::flick_overshoot_px, 0.f, 20.f, "%.1f px");
				ToggleRow("deadzone_enabled", "Dead zone", &cfg::aimbot::dead_zone_enabled);
				if (cfg::aimbot::dead_zone_enabled)
					SliderFloatRow("Dead zone", "##dead_zone", &cfg::aimbot::dead_zone, 0.f, 10.f, "%.1f");
				SliderFloatRow("Stop", "##stop_threshold", &cfg::aimbot::stop_threshold, 0.f, 8.f, "%.1f");
				DetailsOut();
				ImGui::Columns(1);
			}
		}
		else if (active_tab == Tab::TRIGGER) {
			ImGui::Columns(2, "trigger_columns", false);
			Section(Icons::RELOAD, "Trigger activation");
			ToggleRow("trigger", "Triggerbot", &cfg::triggerbot::enabled);
			if (cfg::triggerbot::enabled) {
				DetailsIn();
				KeyCombo("##trigger_key", &cfg::triggerbot::key);
				DetailsOut();

				Section(Icons::SETTINGS, "Trigger behavior");
				DetailsIn();
				ToggleRow("trigger_team", "Target team", &cfg::triggerbot::team);
				ToggleRow("trigger_crosshair", "Crosshair only", &cfg::triggerbot::only_in_crosshair);
				ToggleRow("trigger_random", "Random delay", &cfg::triggerbot::randomization);
				SliderIntRow("Delay", "##trigger_delay", &cfg::triggerbot::delay, 0, 200);
				DetailsOut();
			}

			ImGui::NextColumn();
			Section(Icons::BLIND, "Recoil");
			ToggleRow("rcs", "Recoil control", &cfg::rcs::enabled);
			if (cfg::rcs::enabled) {
				DetailsIn();
				SliderFloatRow("Horizontal", "##rcs_h", &cfg::rcs::horizontal, 0.0f, 2.0f, "%.2f");
				SliderFloatRow("Vertical", "##rcs_v", &cfg::rcs::vertical, 0.0f, 2.0f, "%.2f");
				SliderFloatRow("Smooth", "##rcs_smooth", &cfg::rcs::smooth, 0.0f, 5.0f, "%.2f");
				DetailsOut();
			}

			Section(Icons::SETTINGS, "Utility");
			ToggleRow("antiflash", "Anti-flash", &cfg::antiflash::enabled);
			if (cfg::antiflash::enabled) {
				DetailsIn();
				SliderFloatRow("Opacity", "##flash_opacity", &cfg::antiflash::opacity, 0.0f, 1.0f, "%.2f");
				DetailsOut();
			}
			ImGui::Columns(1);
		}
		else if (active_tab == Tab::VISUALS) {
			ImGui::Columns(2, "visual_columns", false);
			Section(Icons::PERSON, "Player outlines");
			ToggleRow("esp_box", "Box", &cfg::esp::box);
			if (cfg::esp::box) {
				DetailsIn();
				ColorPair("Team##box_team", cfg::esp::colors::box_team.data(), "Enemy##box_enemy", cfg::esp::colors::box_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("esp_skeleton", "Skeleton", &cfg::esp::skeleton);
			if (cfg::esp::skeleton) {
				DetailsIn();
				ColorPair("Team##skeleton_team", cfg::esp::colors::skeleton_team.data(), "Enemy##skeleton_enemy", cfg::esp::colors::skeleton_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("esp_tracker", "Head tracker", &cfg::esp::head_tracker);
			if (cfg::esp::head_tracker) {
				DetailsIn();
				ColorPair("Team##tracker_team", cfg::esp::colors::tracker_team.data(), "Enemy##tracker_enemy", cfg::esp::colors::tracker_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("esp_tracers", "Tracers", &cfg::esp::tracers);
			if (cfg::esp::tracers) {
				DetailsIn();
				ColorPair("Team##tracer_team", cfg::esp::colors::tracer_team.data(), "Enemy##tracer_enemy", cfg::esp::colors::tracer_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("esp_health", "Health", &cfg::esp::health);
			if (cfg::esp::health) {
				DetailsIn();
				ToggleRow("esp_health_number", "Number", &cfg::esp::health_number);
				DetailsOut();
			}
			ToggleRow("esp_armor", "Armor", &cfg::esp::armor);
			ToggleRow("esp_team", "Show team", &cfg::esp::team);
			ToggleRow("esp_spotted", "Spotted only", &cfg::esp::spotted);

			Section(Icons::BLIND, "Player flags");
			ToggleRow("flag_name", "Name", &cfg::esp::flags::name);
			ToggleRow("flag_money", "Money", &cfg::esp::flags::money);
			ToggleRow("flag_weapon", "Weapon", &cfg::esp::flags::weapon);
			ToggleRow("flag_ammo", "Ammo", &cfg::esp::flags::ammo);
			ToggleRow("flag_ping", "Ping", &cfg::esp::flags::ping);
			ToggleRow("flag_flashed", "Flashed", &cfg::esp::flags::flashed);
			if (cfg::esp::flags::flashed) {
				DetailsIn();
				ColorPair("Team##flashed_team", cfg::esp::colors::flags::flashed_team.data(), "Enemy##flashed_enemy", cfg::esp::colors::flags::flashed_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("flag_reloading", "Reloading", &cfg::esp::flags::reloading);
			if (cfg::esp::flags::reloading) {
				DetailsIn();
				ColorPair("Team##reloading_team", cfg::esp::colors::flags::reloading_team.data(), "Enemy##reloading_enemy", cfg::esp::colors::flags::reloading_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("flag_defusing", "Defusing", &cfg::esp::flags::defusing);
			if (cfg::esp::flags::defusing) {
				DetailsIn();
				ColorPair("Team##defusing_team", cfg::esp::colors::flags::defusing_team.data(), "Enemy##defusing_enemy", cfg::esp::colors::flags::defusing_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("flag_scoped", "Scoped", &cfg::esp::flags::scoped);
			if (cfg::esp::flags::scoped) {
				DetailsIn();
				ColorPair("Team##scoped_team", cfg::esp::colors::flags::scoped_team.data(), "Enemy##scoped_enemy", cfg::esp::colors::flags::scoped_enemy.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("flag_c4", "Has C4", &cfg::esp::flags::has_c4);
			if (cfg::esp::flags::has_c4) {
				DetailsIn();
				ColorPair("Team##c4_team", cfg::esp::colors::flags::c4_team.data(), "Enemy##c4_enemy", cfg::esp::colors::flags::c4_enemy.data(), color_flags);
				DetailsOut();
			}
			ImGui::Columns(1);
		}
		else if (active_tab == Tab::WORLD) {
			ImGui::Columns(2, "world_columns", false);
			Section(Icons::GLOBE, "Bomb");
			ToggleRow("bomb_box", "Bomb ESP", &cfg::esp::bomb);
			if (cfg::esp::bomb) {
				DetailsIn();
				ColorSingle("Color", cfg::esp::colors::bomb.data(), color_flags);
				DetailsOut();
			}
			ToggleRow("bomb_location", "Bomb location", &cfg::world::bomb::location);
			ToggleRow("bomb_timer", "Bomb timer", &cfg::world::bomb::timer);
			ToggleRow("bomb_hud", "Bomb HUD", &cfg::world::bomb::hud);

			ImGui::NextColumn();
			Section(Icons::GLOBE, "World overlays");
			ToggleRow("spectators", "Spectator list", &cfg::world::spectators::enabled);
			if (cfg::world::spectators::enabled) {
				DetailsIn();
				ToggleRow("spectators_detailed", "Detailed", &cfg::world::spectators::detailed);
				ToggleRow("spectators_self", "Only self", &cfg::world::spectators::self_only);
				DetailsOut();
			}
			ToggleRow("crosshair", "Crosshair", &cfg::world::crosshair::enabled);
			ToggleRow("radar", "Radar", &cfg::world::radar::enabled);
			if (cfg::world::radar::enabled) {
				DetailsIn();
				SliderFloatRow("Range", "##radar_range", &cfg::world::radar::range, 100.f, 8000.f, "%.0f u");
				ToggleRow("radar_rotate", "No rotate", &cfg::world::radar::no_rotate);
				DetailsOut();
			}
			ToggleRow("velocity", "Velocity graph", &cfg::world::velocity::enabled);
			if (cfg::world::velocity::enabled) {
				DetailsIn();
				SliderIntRow("Sample", "##velocity_sample", &cfg::world::velocity::sample_rate, 5, 120);
				SliderFloatRow("Length", "##velocity_length", &cfg::world::velocity::sample_length, 1.f, 15.f, "%.1f");
				DetailsOut();
			}
			ImGui::Columns(1);
		}
		else if (active_tab == Tab::SETTINGS) {
			Section(Icons::SETTINGS, "Runtime");
			if (ToggleRow("streamproof", "Streamproof", &cfg::settings::streamproof))
				Window::SetAffinity(Window::hwnd, cfg::settings::streamproof ? WindowAffinity::Invisible : WindowAffinity::Disabled);
			ToggleRow("watermark", "Watermark", &cfg::settings::watermark);
			if (ToggleRow("vsync", "VSync", &cfg::settings::vsync))
				Window::vsync = cfg::settings::vsync;
			ToggleRow("free_cpu", "Free CPU", &cfg::settings::free_cpu);
			SliderIntRow("Update", "##update_rate", &cfg::settings::update_rate, 1, 120);

#ifdef _DEBUG
			Section(Icons::GITLAB, "Dev");
			if (ToggleRow("console", "Console", &cfg::dev::console))
				if (!cfg::dev::console) LogHelper::Free();
			SliderIntRow("Cache", "##cache_refresh_rate", &cfg::dev::cache_refresh_rate, 0, 100);
			ToggleRow("force_flags", "Force show flags", &cfg::dev::force_show_flags);
#endif
		}

		ImGui::EndChild();

		if (ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			pending_auto_save = true;
			last_edit_time = ImGui::GetTime();
		}

		if (pending_auto_save && ImGui::GetTime() - last_edit_time > 0.5) {
			Config::Write();
			pending_auto_save = false;
		}
	}

	ImGui::End();
}

void Menu::SetupStyles() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = ui.text;
	style.Colors[ImGuiCol_TextDisabled] = ui.dim;
	style.Colors[ImGuiCol_WindowBg] = ui.bg;
	style.Colors[ImGuiCol_ChildBg] = ui.bg;
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.050f, 0.054f, 0.062f, 0.99f);
	style.Colors[ImGuiCol_Border] = ui.line;
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.064f, 0.070f, 0.082f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered] = ui.hover;
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.090f, 0.118f, 0.132f, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ui.bg;
	style.Colors[ImGuiCol_TitleBgActive] = ui.bg;
	style.Colors[ImGuiCol_TitleBgCollapsed] = ui.bg;
	style.Colors[ImGuiCol_MenuBarBg] = ui.side;
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.045f, 0.048f, 0.055f, 0.70f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.25f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ui.accent;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ui.good;
	style.Colors[ImGuiCol_CheckMark] = ui.accent;
	style.Colors[ImGuiCol_SliderGrab] = ui.accent;
	style.Colors[ImGuiCol_SliderGrabActive] = ui.good;
	style.Colors[ImGuiCol_Button] = ImVec4(0.064f, 0.070f, 0.082f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered] = ui.hover;
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.090f, 0.118f, 0.132f, 1.00f);
	style.Colors[ImGuiCol_Header] = ui.hover;
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.088f, 0.098f, 0.112f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.090f, 0.118f, 0.132f, 1.00f);
	style.Colors[ImGuiCol_Separator] = ui.line;
	style.Colors[ImGuiCol_SeparatorHovered] = ui.accent;
	style.Colors[ImGuiCol_SeparatorActive] = ui.accent;
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ui.hover;
	style.Colors[ImGuiCol_ResizeGripActive] = ui.accent;
	style.Colors[ImGuiCol_Tab] = ui.side;
	style.Colors[ImGuiCol_TabHovered] = ui.hover;
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.090f, 0.118f, 0.132f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocused] = ui.bg;
	style.Colors[ImGuiCol_TabUnfocusedActive] = ui.side;
	style.Colors[ImGuiCol_PlotLines] = ui.accent;
	style.Colors[ImGuiCol_PlotLinesHovered] = ui.good;
	style.Colors[ImGuiCol_PlotHistogram] = ui.accent;
	style.Colors[ImGuiCol_PlotHistogramHovered] = ui.good;
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.36f, 0.58f, 0.92f, 0.25f);
	style.Colors[ImGuiCol_DragDropTarget] = ui.accent;
	style.Colors[ImGuiCol_NavHighlight] = ui.accent;
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.45f);

	style.FrameBorderSize = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.DisabledAlpha = 0.46f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.WindowRounding = 7.f;
	style.ChildRounding = 0.f;
	style.FrameRounding = 4.f;
	style.PopupRounding = 6.f;
	style.GrabRounding = 4.f;
	style.ScrollbarRounding = 6.f;
	style.ScrollbarSize = 8.0f;
	style.WindowPadding = ImVec2(14, 12);
	style.FramePadding = ImVec2(7, 4);
	style.ItemSpacing = ImVec2(7, 5);
	style.ItemInnerSpacing = ImVec2(6, 4);

	auto& io = ImGui::GetIO();
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f);

	ImFontConfig merge_icon_cfg{};
	merge_icon_cfg.FontDataOwnedByAtlas = false;
	merge_icon_cfg.MergeMode = true;
	merge_icon_cfg.GlyphOffset = Vec2_t(0, 3.0f);

	static const ImWchar icon_ranges[] = { 0xE100, 0xE108, 0 };
	io.Fonts->AddFontFromMemoryTTF(icons_font, icons_font_len, 18.f, &merge_icon_cfg, icon_ranges);
}

void Menu::RenderStartupHelpImpl() {
	static bool has_opened_menu = false;

	if (has_opened_menu)
		return;

	auto& io = ImGui::GetIO();
	auto screen = io.DisplaySize;
	auto d = ImGui::GetBackgroundDrawList();

	if (Renderer::IsOpen())
		has_opened_menu = true;

	auto help = "Insert / Right Shift to open  |  End to close";
	auto size = ImGui::CalcTextSize(help);
	ImVec2 pos(screen.x / 2 - size.x / 2 - 10.0f, 78.0f);
	ImVec2 box(size.x + 20.0f, size.y + 10.0f);
	d->AddRectFilled(pos, pos + box, Color(ui.bg, 0.78f), 5.0f);
	d->AddText(pos + ImVec2(10.0f, 5.0f), Color(ui.text), help);
}
