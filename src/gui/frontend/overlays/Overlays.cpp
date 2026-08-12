#include "Overlays.hpp"

#include "gui/renderer/Renderer.hpp" // Circular dependency
#include "gui/frontend/menu/Menu.hpp" // Circular dependency
#include "assets/fonts/WeaponIcons.h"

#include <cstdio>

bool Overlays::Init() {
    return GetInstance().InitImpl();
}

void Overlays::Render() {
    return GetInstance().Render(Cache::CopySnapshot());
}

void Overlays::Render(const Snapshot& snapshot) {
    return GetInstance().RenderImpl(snapshot);
}

bool Overlays::InitImpl() {
    auto& io = ImGui::GetIO();

    ImFontConfig cfg{};
    cfg.FontDataOwnedByAtlas = false;

	this->font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 12.0f, &cfg);
	this->font_alt = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 14.0f, &cfg);
    this->font_icons = io.Fonts->AddFontFromMemoryTTF(weapon_icon_font, weapon_icon_font_len, 16.0f,  &cfg);
	
	ImFontConfig merge_icon_cfg{};
	merge_icon_cfg.FontDataOwnedByAtlas = false;
	merge_icon_cfg.MergeMode = true;

	static const ImWchar icon_ranges[] = { 0xE000, 0xE046, 0 };
	io.Fonts->AddFontFromMemoryTTF(weapon_icon_font, weapon_icon_font_len, 12.f, &merge_icon_cfg, icon_ranges);

    // Pre allocate buffer
    this->vel_buffer.resize(static_cast<size_t>(cfg::world::velocity::sample_rate * cfg::world::velocity::sample_length));

    return true;
}

void Overlays::RenderImpl(const Snapshot& snapshot) {
    ImGui::PushFont(this->font);
    {
        RenderWatermark(snapshot);

        RenderNotice();

    #ifdef _DEBUG
        RenderDebugWindow(snapshot);
    #endif

    }
    ImGui::PopFont();

    ImGui::PushFont(this->font_alt);
    {
        RenderSpectatorList(snapshot);
        RenderSpeedChart(snapshot);
        RenderRadar(snapshot);
        RenderBomb(snapshot);
    }
    ImGui::PopFont();
}

void Overlays::RenderWatermark(const Snapshot& snapshot) {
    if (!cfg::settings::watermark)
        return;

    auto& io = ImGui::GetIO();

    auto& globals = snapshot.globals;

    static int margin = 10;
    std::string watermark_string = "cs2-external-esp";

    char fps_text[32]{};
    snprintf(fps_text, sizeof(fps_text), " | %dfps", static_cast<int>(io.Framerate));
    watermark_string += fps_text;

    if (globals.in_match) {
        watermark_string += " | ";
        watermark_string += globals.map_name;
    }

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("##watermark", nullptr, flags))
        ImGui::TextUnformatted(watermark_string.c_str());

    ImGui::End();
}

void Overlays::RenderNotice() {
}

inline const Player* FindPlayerByPawnIndex(const std::vector<Player>& players, int index) {
    const Player* found = nullptr;

    for (auto& p : players) {
        if (p.pawn_controller_addr == index) {
            found = &p;
            break;
        }
    }
    return found;
}

void Overlays::RenderSpectatorList(const Snapshot& snapshot) {
    if (!cfg::world::spectators::enabled)
        return;

    auto& players = snapshot.players;

    const bool is_menu_open = Renderer::IsOpen();
    const bool detailed = cfg::world::spectators::detailed;
    const bool self_only = cfg::world::spectators::self_only;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    ImGuiTableFlags flags_table = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_BordersV;

    bool should_render = false;
    for (const Player& p : players) {
        if (auto i = p.observer_services.target) {
            const Player* target = FindPlayerByPawnIndex(players, i);

            if (self_only && (!target || !target->localplayer))
                continue;

            should_render = true;
            break;
        }
    }

    if (!should_render && !is_menu_open)
        return;

    // Window
    ImGui::SetNextWindowPos(cfg::world::spectators::pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(150.f, 50.f), ImVec2(FLT_MAX, FLT_MAX));

    if (!ImGui::Begin("Spectator list", nullptr, flags)) {
        ImGui::End();
        return;
    }

    if (is_menu_open)
        cfg::world::spectators::pos = ImGui::GetWindowPos();

    if (!should_render && is_menu_open) {
        ImGui::TextDisabled("No spectators");
        return ImGui::End();
    }

    if (detailed) {
        if (ImGui::BeginTable("##detailed", 3, flags_table)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Mode");
            ImGui::TableSetupColumn("Target");
            ImGui::TableHeadersRow();

            for (const Player& player : players) {
                if (player.alive) continue;

                int targetIndex = player.observer_services.target;
                if (targetIndex == 0) continue;

                const Player* target = FindPlayerByPawnIndex(players, targetIndex);

                if (self_only && (!target || !target->localplayer))
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", player.name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", player.observer_services.ToString());

                ImGui::TableSetColumnIndex(2);
                if (self_only) ImGui::Text("You");
                else if (player.observer_services.mode == ObserverMode::Free) ImGui::Text("No One");
                else ImGui::Text("%s", target ? target->name : "Invalid/bomb");
            }

            ImGui::EndTable();
        }
    }
    else {
        for (const Player& player : players) {
            if (player.alive) continue;
            int targetIndex = player.observer_services.target;
            if (targetIndex == 0) continue;
            const Player* target = FindPlayerByPawnIndex(players, targetIndex);

            if (self_only && (!target || !target->localplayer)) continue;

            ImGui::Text("%s", player.name);
        }
    }

    ImGui::End();
}

void Overlays::RenderSpeedChart(const Snapshot& snapshot) {
    if (!cfg::world::velocity::enabled)
        return;

    auto& io = ImGui::GetIO();

    auto& local = snapshot.local;

    const bool is_menu_open = Renderer::IsOpen();

    auto& pos = cfg::world::velocity::pos;
    auto& size = cfg::world::velocity::size;

    int rate = cfg::world::velocity::sample_rate;
    float length = cfg::world::velocity::sample_length;

    static int prev_rate = rate;
    static float prev_length = length;

    if (!is_menu_open && !local.alive)
        return;

    // Cache menu values and resize when changed
    if (prev_rate != rate || prev_length != length) {
        prev_rate = rate;
        prev_length = length;

        vel_buffer.resize(static_cast<size_t>(rate * length));
    }

    Vec2_t speed_2d(local.vel.x, local.vel.y);
    int speed = floor(speed_2d.len());

    vel_accumulator += io.DeltaTime;
    size_t buff_size = vel_buffer.size();
    if (buff_size == 0)
        return;

    float sample_interval = 1.0f / rate;

    while (vel_accumulator >= sample_interval)
    {
        vel_accumulator -= sample_interval;
        vel_buffer.at(vel_index % buff_size) = speed;
        vel_index = (vel_index + 1) % buff_size;
    }

    int max_speed = 1;
    for (int v : vel_buffer)
        max_speed = std::max(max_speed, v);

    static std::vector<float> plot_values;
    plot_values.resize(buff_size);
    for (size_t i = 0; i < buff_size; ++i)
        plot_values[i] = static_cast<float>(vel_buffer[(i + vel_index) % buff_size]);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
    if (!is_menu_open)
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowBgAlpha(is_menu_open ? 0.82f : 0.35f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (ImGui::Begin("Velocity Graph", nullptr, flags))
    {
        if (is_menu_open) {
            pos = ImGui::GetWindowPos();
            size = ImGui::GetWindowSize();
        }

        ImGui::Text("Speed: %d", speed);
        ImGui::PlotLines(
            "##velocity_plot",
            plot_values.data(),
            static_cast<int>(plot_values.size()),
            0,
            nullptr,
            0.0f,
            static_cast<float>(max_speed),
            ImVec2(-1.0f, ImGui::GetContentRegionAvail().y)
        );
    }
    ImGui::End();
}

#ifdef _DEBUG
void Overlays::RenderDebugWindow(const Snapshot& snapshot) {
    auto& game = snapshot.game;
    auto& bomb = snapshot.bomb;
    auto& globals = snapshot.globals;
    auto& players = snapshot.players;

    std::string debug_string = "> Game Debug Window\n";

    debug_string += std::format("Map: {}\n", globals.map_name);
    debug_string += std::format("Max Clients: {}\n", globals.max_clients);
    debug_string += std::format("Cache Refresh: {}ms\n", cfg::dev::cache_refresh_rate);

    if (bomb.is_planted) {
        debug_string += "Bomb:\n";
        debug_string += std::format("- Planted Site: {}\n", bomb.site == BombSite::A ? "A" : "B");
    }

    if (!players.empty())
        debug_string += std::format("Players ({}):\n", players.size());

	for (auto& player : players)
		debug_string += std::format(
			"- [{}] {} {}hp {} {}\n", 
			player.index, player.name, 
			player.health, player.weapon.name,
			player.weapon.icon
		);

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    if (ImGui::Begin("Game Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        ImGui::TextUnformatted(debug_string.c_str());

    ImGui::End();
}
#endif

void Overlays::RenderRadar(const Snapshot& snapshot) {
    if (!cfg::world::radar::enabled)
        return;

    auto& local = snapshot.local;
    auto& players = snapshot.players;

    const bool is_menu_open = Renderer::IsOpen();

    if (!is_menu_open && !local.alive)
        return;

    auto& pos = cfg::world::radar::pos;
    auto& size = cfg::world::radar::size;
    float range = cfg::world::radar::range;

    if (is_menu_open) {
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Radar", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
            pos = ImGui::GetWindowPos();
            size = ImGui::GetWindowSize();
            ImGui::End();
        }
    }

    auto d = ImGui::GetBackgroundDrawList();

    const float cx = pos.x + size.x * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    const float rx = size.x * 0.5f;
    const float ry = size.y * 0.5f;
    const float radius = std::min(rx, ry);

    d->AddRectFilled(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(0, 0, 0, 50),
        6.f
    );

    d->AddRect(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(80, 80, 80, 100),
        6.f
    );

    d->AddCircle(ImVec2(cx, cy), radius * 0.333f, IM_COL32(50, 50, 50, 120));
    d->AddCircle(ImVec2(cx, cy), radius * 0.666f, IM_COL32(50, 50, 50, 120));
    d->AddLine(ImVec2(pos.x + 4.f, cy), ImVec2(pos.x + size.x - 4.f, cy), IM_COL32(50, 50, 50, 120));
    d->AddLine(ImVec2(cx, pos.y + 4.f), ImVec2(cx, pos.y + size.y - 4.f), IM_COL32(50, 50, 50, 120));

    for (auto& player : players) {
        if (!player.alive)
            continue;

        if (player.localplayer)
            continue;

        Vec3_t delta = player.pos - local.pos;
        float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (dist > range)
            continue;

        float nx = delta.x / range;
        float ny = delta.y / range;

        float sx, sy;
        if (!cfg::world::radar::no_rotate) {
            const auto& matrix = snapshot.game.view_matrix;
            float rx = matrix[0][0];
            float ry = matrix[0][1];
            float len = sqrtf(rx * rx + ry * ry);
            if (len > 0.001f) { rx /= len; ry /= len; }
            float fx = -ry;
            float fy =  rx;
            float rad_x = nx * rx + ny * ry;
            float rad_y = nx * fx + ny * fy;
            sx = cx + rad_x * (size.x * 0.5f - 6.f);
            sy = cy - rad_y * (size.y * 0.5f - 6.f);
        } else {
            sx = cx + nx * (size.x * 0.5f - 6.f);
            sy = cy - ny * (size.y * 0.5f - 6.f);
        }

        bool mate = player.team == local.team;
        ImU32 col = mate
            ? IM_COL32(0, 220, 80, 255)
            : IM_COL32(220, 50, 50, 255);

        d->AddCircleFilled(ImVec2(sx, sy), 4.f, col);
        d->AddCircle(ImVec2(sx, sy), 4.f, IM_COL32(0, 0, 0, 180));
    }

    d->AddCircleFilled(ImVec2(cx, cy), 5.f, IM_COL32(100, 180, 255, 255));
    d->AddCircle(ImVec2(cx, cy), 5.f, IM_COL32(0, 0, 0, 180));

    d->AddText(ImVec2(pos.x + 6.f, pos.y + 4.f), IM_COL32(180, 180, 180, 200), "Radar");
}

void Overlays::RenderBomb(const Snapshot& snapshot) {
    if (!cfg::world::bomb::location && !cfg::world::bomb::timer)
        return;

    auto& io = ImGui::GetIO();

    auto& bomb = snapshot.bomb;
    auto& local = snapshot.local;

    const bool is_menu_open = Renderer::IsOpen();

    float width = 20.f;
    float height = 20.f;

    static int margin = 4;
    static int padding = 6;
    float bar_height = 3.f;
    float element_gap = 6.f;

    char duration_str[16]{};
    snprintf(duration_str, sizeof(duration_str), "%.0fs", bomb.is_planted ? bomb.time_left : 40.0f);
    auto bombsite_str = std::string(!bomb.is_planted || bomb.site == BombSite::A ? "A" : "B");

    std::string bomb_string = "";

    if (cfg::world::bomb::location)
        bomb_string += "SITE " + bombsite_str;

    if (cfg::world::bomb::timer)
    {
        if (cfg::world::bomb::location)
            bomb_string += " | ";

        bomb_string += duration_str;
    }

    auto text_size = ImGui::CalcTextSize(bomb_string.data());

    ImGui::PushFont(this->font_icons);
    auto icon_size = ImGui::CalcTextSize(WeaponIcons::C4);
    ImGui::PopFont();

    float content_width = icon_size.x + element_gap + text_size.x;
    float content_height = std::max(icon_size.y, text_size.y);

    width = content_width + (padding * 2);
    height = content_height + (padding * 2) + (cfg::world::bomb::timer ? bar_height + 2.f : 0.f);

    if (!bomb.is_planted && !is_menu_open)
        return;

    if (bomb.is_planted && !bomb.pos.length() && !is_menu_open)
        return;

    if (!local.alive && !is_menu_open)
        return;

    float render_x = cfg::world::bomb::pos.x;
    float render_y = cfg::world::bomb::pos.y;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (!is_menu_open)
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar;

    ImGui::SetNextWindowBgAlpha(0.86f);
    ImGui::SetNextWindowPos(ImVec2(render_x, render_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height + (is_menu_open ? ImGui::GetFrameHeight() : 0.0f)), ImGuiCond_Always);
    if (ImGui::Begin("Bomb Window", nullptr, flags))
    {
        if (is_menu_open)
            cfg::world::bomb::pos = ImGui::GetWindowPos();

        ImGui::PushFont(this->font_icons);
        ImGui::TextColored(ImVec4(1.0f, 0.24f, 0.24f, 1.0f), "%s", WeaponIcons::C4);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextUnformatted(bomb_string.c_str());

        if (cfg::world::bomb::timer)
        {
            float time_left = bomb.is_planted ? bomb.time_left : 40.f;
            float progress = std::clamp(time_left / 40.f, 0.f, 1.f);
            ImGui::ProgressBar(progress, ImVec2(-1.0f, bar_height + 5.0f), "");
        }
    }
    ImGui::End();
}
