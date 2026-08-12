#include "Esp.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"
#include "core/vischeck/VisCheckManager.h"
#include "gui/renderer/Renderer.hpp"
#include "assets/fonts/WeaponIcons.h"
#include "assets/fonts/Icons.h"

#include <algorithm>
#include <cstdio>

bool Esp::Init() {
	return GetInstance().InitImpl();
}

void Esp::Render() {
    return GetInstance().Render(Cache::CopySnapshot());
}

void Esp::Render(const Snapshot& snapshot) {
	return GetInstance().RenderImpl(snapshot);
}

bool Esp::InitImpl() {
	auto& io = ImGui::GetIO();

	ImFontConfig cfg{};
	cfg.FontDataOwnedByAtlas = false;

	this->font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 12.0f, &cfg);

	this->font_merged_icons = io.Fonts->AddFontFromMemoryTTF(
		weapon_icon_font,
		weapon_icon_font_len,
		16.0f,
		&cfg
	);

	cfg.MergeMode = true;

	static const ImWchar general_ranges[] = { 0xE100, 0xE108, 0 };
	io.Fonts->AddFontFromMemoryTTF(
		icons_font,
		icons_font_len,
		16.0f,
		&cfg,
		general_ranges
	);

	sound_markers.reserve(96);
	hit_markers.reserve(32);
	tracks.reserve(64);

	return true;
}

void Esp::RenderImpl(const Snapshot& snapshot) {
	if (!cfg::enabled)
		return;

	auto& game = snapshot.game;
	auto& bomb = snapshot.bomb;
	auto& local = snapshot.local;
	auto& globals = snapshot.globals;
	auto& players = snapshot.players;
	
	ImGui::PushFont(this->font);

	this->io = ImGui::GetIO();
	this->d = ImGui::GetBackgroundDrawList();
	this->render_size = GetProjectionSize();

	auto now = std::chrono::steady_clock::now();
	Player render_local = local;

	this->matrix = game.view_matrix;
	if (auto process = Engine::GetProcess()) {
		auto client = Engine::GetClient();
		if (client.base && offsets::viewMatrix)
			this->matrix = process->read<view_matrix_t>(client.base + offsets::viewMatrix);
	}

	auto add_sound_marker = [&](const SoundMarker& marker) {
		if (sound_markers.size() >= 96)
			sound_markers.erase(sound_markers.begin());
		sound_markers.push_back(marker);
	};

	auto add_hit_marker = [&](const HitMarker& marker) {
		if (hit_markers.size() >= 32)
			hit_markers.erase(hit_markers.begin());
		hit_markers.push_back(marker);
	};

	for (auto& player : players) {
		if (!player.alive)
			continue;
		if (player.localplayer)
			continue;

		bool mate = player.team == local.team;

		auto& track = tracks[player.index];
		track.last_seen = now;

		if (cfg::esp::sound) {
			if (!cfg::esp::team && mate)
				goto skip_sound;

			if (cfg::esp::sound_footsteps) {
				float speed = player.vel.length();
				constexpr float step_threshold = 5.0f;
				if (speed > step_threshold) {
					auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
						now - track.last_footstep
					).count();
					if (elapsed > 0.35f) {
						track.last_footstep = now;
						add_sound_marker({ player.pos, now, mate, 0 });
					}
				}
			}

			if (cfg::esp::sound_gunfire && player.ammo != -1) {
				if (track.last_ammo >= 0 && player.ammo < track.last_ammo) {
					add_sound_marker({ player.pos, now, mate, 1 });
				}
			}

			if (cfg::esp::sound_gunfire) {
				if (player.is_reloading && !track.was_reloading) {
					add_sound_marker({ player.pos, now, mate, 2 });
				}
				track.was_reloading = player.is_reloading;
			}
		}
		skip_sound:

		if (cfg::esp::hit_markers) {
			if (track.last_health > 0 && player.health < track.last_health) {
				int damage = track.last_health - player.health;
				Vec3_t hit_pos = player.pos;
				if ((int)player.bone_list.size() > bone_index::chest)
					hit_pos = player.bone_list[bone_index::chest].pos;
				add_hit_marker({ hit_pos, now, mate, damage });
			}
		}

		if (cfg::esp::hit_markers)
			track.last_health = player.health;
		if (cfg::esp::sound && cfg::esp::sound_gunfire)
			track.last_ammo = static_cast<float>(player.ammo);
	}

	if (cfg::esp::sound) {
		auto fade_duration = std::chrono::duration<float>(cfg::esp::sound_fade);
		for (auto it = sound_markers.begin(); it != sound_markers.end(); ) {
			auto age = std::chrono::duration_cast<std::chrono::duration<float>>(now - it->birth);
			if (age >= fade_duration) {
				it = sound_markers.erase(it);
				continue;
			}

			Vec2_t screen;
			if (matrix.wts(it->pos, render_size, screen)) {
				float alpha_ratio = 1.0f - (age.count() / cfg::esp::sound_fade);
				color_t base_color;
				switch (it->kind) {
					case 0: base_color = it->mate ? cfg::esp::colors::sound::footstep_team : cfg::esp::colors::sound::footstep_enemy; break;
					case 1: base_color = it->mate ? cfg::esp::colors::sound::gunfire_team : cfg::esp::colors::sound::gunfire_enemy; break;
					case 2: base_color = it->mate ? cfg::esp::colors::sound::reload_team : cfg::esp::colors::sound::reload_enemy; break;
					default: base_color = it->mate ? cfg::esp::colors::sound::footstep_team : cfg::esp::colors::sound::footstep_enemy; break;
				}
				ImColor col(base_color.r, base_color.g, base_color.b, base_color.a * alpha_ratio);

				float radius = 10.0f + (1.0f - alpha_ratio) * 15.0f;
				int segments = 12;
				d->AddCircle(screen, radius, col, segments);
				d->AddCircle(screen, radius * 0.65f, col, segments);

				const char* label;
				switch (it->kind) {
					case 0: label = "step"; break;
					case 1: label = "fire"; break;
					case 2: label = "reload"; break;
					default: label = "?"; break;
				}
				auto txt_sz = ImGui::CalcTextSize(label);
				d->AddText(
					Vec2_t(screen.x - txt_sz.x * 0.5f, screen.y - radius - txt_sz.y - 2),
					ImColor(1.0f, 1.0f, 1.0f, alpha_ratio),
					label
				);
			}
			++it;
		}
	}

	if (cfg::esp::hit_markers) {
		auto hm_fade = std::chrono::duration<float>(cfg::esp::hit_marker_fade);
		for (auto it = hit_markers.begin(); it != hit_markers.end(); ) {
			auto age = std::chrono::duration_cast<std::chrono::duration<float>>(now - it->birth);
			if (age >= hm_fade) {
				it = hit_markers.erase(it);
				continue;
			}

			Vec2_t screen;
			if (matrix.wts(it->pos, render_size, screen)) {
				float alpha = 1.0f - (age.count() / cfg::esp::hit_marker_fade);
				auto col = cfg::esp::colors::hit_marker;
				ImColor c(col.r, col.g, col.b, col.a * alpha);
				constexpr float size = 5.f;

				d->AddLine(
					Vec2_t(screen.x - size, screen.y - size),
					Vec2_t(screen.x + size, screen.y + size),
					c, std::max(1.0f, cfg::esp::skeleton_thickness)
				);
				d->AddLine(
					Vec2_t(screen.x + size, screen.y - size),
					Vec2_t(screen.x - size, screen.y + size),
					c, std::max(1.0f, cfg::esp::skeleton_thickness)
				);

				auto txt = std::to_string(it->damage);
				auto txt_sz = ImGui::CalcTextSize(txt.c_str());
				d->AddText(
					Vec2_t(screen.x - txt_sz.x * 0.5f, screen.y - size - txt_sz.y - 2),
					ImColor(1.f, 1.f, 1.f, alpha),
					txt.c_str()
				);
			}
			++it;
		}
	}

	const Vec3_t eye_pos = render_local.bone_list.size() > bone_index::head
		? render_local.bone_list[bone_index::head].pos
		: render_local.pos + Vec3_t(0, 0, 64.f);
	const bool los_filter_enabled = cfg::esp::spotted || cfg::esp::los_spotted;
	const bool los_ready = los_filter_enabled && VisCheckManager::IsReady();
	static uint8_t vis_hold[64]{};

	for (auto& player : players) {
		if (!player.alive)
			continue;

		if (player.localplayer)
			continue;

		Player render_player = player;

		bool mate = render_player.team == render_local.team;

		if (!cfg::esp::team && mate)
			continue;

		if (
			render_local.observer_services.target == render_player.pawn_controller_addr
			&& render_local.observer_services.mode == ObserverMode::First
		)
			continue;

		bool los_visible = false;
		if (los_ready && render_player.bone_list.size() > bone_index::pelvis) {
			const auto& bones = render_player.bone_list;
			std::array<Vec3_t, 5> los_targets{};
			size_t los_target_count = 0;
			los_targets[los_target_count++] = bones[bone_index::head].pos;
			los_targets[los_target_count++] = bones[bone_index::chest].pos;
			if (cfg::esp::los_extra_bones) {
				los_targets[los_target_count++] = bones[bone_index::shoulder_L].pos;
				los_targets[los_target_count++] = bones[bone_index::shoulder_R].pos;
				los_targets[los_target_count++] = bones[bone_index::pelvis].pos;
			}

			const bool has_los = VisCheckManager::IsAnyVisible(
				eye_pos,
				std::span<const Vec3_t>(los_targets.data(), los_target_count)
			);

			auto& h = vis_hold[render_player.index & 63];
			if (has_los)
				h = 6;
			else if (h)
				--h;

			los_visible = h > 0;
		}

		if (los_filter_enabled && los_ready && !los_visible)
			continue;

		render_player = SmoothPlayer(render_player, tracks[render_player.index], now);

		RenderPlayerTracers(render_local, render_player, mate);
		RenderPlayer(render_player, mate, los_visible);
	}

	for (auto it = tracks.begin(); it != tracks.end(); ) {
		if (now - it->second.last_seen > std::chrono::seconds(3))
			it = tracks.erase(it);
		else
			++it;
	}

	RenderCrosshair(render_local);
	RenderMapWireframe(render_local);
	RenderBombBox(bomb);
	ImGui::PopFont();
}

Vec2_t Esp::GetProjectionSize() {
	auto size = ImGui::GetIO().DisplaySize;
	if (size.x < 1.0f || size.y < 1.0f)
		return Vec2_t(1.0f, 1.0f);

	return size;
}

static Vec3_t LerpVec3(const Vec3_t& from, const Vec3_t& to, float amount) {
	return from + (to - from) * amount;
}

Player Esp::SmoothPlayer(Player player, PlayerTrack& track, std::chrono::steady_clock::time_point now) {
	if (!cfg::esp::smoothing)
		return player;

	const float speed = player.vel.length();
	const float smooth_rate = cfg::esp::smoothing_speed + std::clamp(speed / 18.0f, 0.0f, 35.0f);
	constexpr float teleport_distance_sqr = 160.0f * 160.0f;

	float dt = io.DeltaTime;
	if (track.last_smooth.time_since_epoch().count() != 0) {
		dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - track.last_smooth).count();
	}
	track.last_smooth = now;

	float amount = 1.0f - std::exp(-smooth_rate * std::max(dt, 0.0f));
	amount = std::clamp(amount, 0.0f, 1.0f);

	const bool reset =
		!track.smooth_initialized ||
		track.smooth_pos.zero() ||
		(player.pos - track.smooth_pos).length_sqr() > teleport_distance_sqr;

	if (reset) {
		track.smooth_initialized = true;
		track.smooth_pos = player.pos;
		track.smooth_bones.clear();
		track.smooth_bones.reserve(player.bone_list.size());
		for (const auto& bone : player.bone_list)
			track.smooth_bones.push_back(bone.pos);
		return player;
	}

	track.smooth_pos = LerpVec3(track.smooth_pos, player.pos, amount);
	player.pos = track.smooth_pos;

	if (track.smooth_bones.size() != player.bone_list.size()) {
		track.smooth_bones.clear();
		track.smooth_bones.reserve(player.bone_list.size());
		for (const auto& bone : player.bone_list)
			track.smooth_bones.push_back(bone.pos);
	}
	else {
		for (size_t i = 0; i < player.bone_list.size(); ++i) {
			if ((player.bone_list[i].pos - track.smooth_bones[i]).length_sqr() > teleport_distance_sqr)
				track.smooth_bones[i] = player.bone_list[i].pos;
			else
				track.smooth_bones[i] = LerpVec3(track.smooth_bones[i], player.bone_list[i].pos, amount);
			player.bone_list[i].pos = track.smooth_bones[i];
		}
	}

	return player;
}

void Esp::RenderPlayer(Player player, bool mate, bool visible) {
	std::pair<Vec2_t, Vec2_t> bounds;
	if (!player.GetBounds(matrix, render_size, bounds))
		return;
	if (!player.alive)
		return;

	const bool use_vis = visible && cfg::esp::los_spotted && cfg::esp::los_use_visible_colors;

	if (cfg::esp::box) {
		auto color = mate ? cfg::esp::colors::box_team : cfg::esp::colors::box_enemy;
		if (use_vis)
			color = mate ? cfg::esp::colors::los_visible_team : cfg::esp::colors::los_visible_enemy;
		d->AddRect(bounds.first, bounds.second, ImColor(color), 0.0f, 0, cfg::esp::box_thickness);
	}

	if (cfg::esp::box && cfg::esp::box_3d)
		RenderPlayerBox3D(player, bounds, mate, use_vis);

	if (cfg::esp::skeleton)
		RenderPlayerBones(player, mate, use_vis);

	if (cfg::esp::head_tracker)
		RenderPlayerTracker(player, bounds, mate, use_vis);

	RenderPlayerBars(player, bounds);
	RenderPlayerFalgs(player, bounds, mate);
}

void Esp::RenderPlayerBones(Player player, bool mate, bool use_vis) {
	auto color = mate ? cfg::esp::colors::skeleton_team : cfg::esp::colors::skeleton_enemy;
	if (use_vis)
		color = mate ? cfg::esp::colors::los_visible_team : cfg::esp::colors::los_visible_enemy;

	auto bone_count = player.bone_list.size();
	ImColor draw_color(color);
	std::array<Vec2_t, 30> projected{};
	std::array<bool, 30> projected_ok{};

	for (const auto& bone : connections) {
		int first = bone[0], second = bone[1];

		if (bone_count <= first || bone_count <= second)
			continue;

		if (!projected_ok[first])
			projected_ok[first] = matrix.wts(player.bone_list[first].pos, render_size, projected[first]);
		if (!projected_ok[second])
			projected_ok[second] = matrix.wts(player.bone_list[second].pos, render_size, projected[second]);
		if (!projected_ok[first] || !projected_ok[second])
			continue;

		d->AddLine(
			projected[first],
			projected[second],
			draw_color,
			cfg::esp::skeleton_thickness
		);
	}

	if (cfg::esp::skeleton_joints) {
		for (size_t i = 0; i < projected_ok.size(); ++i)
			if (projected_ok[i])
				d->AddCircleFilled(projected[i], cfg::esp::joint_radius, draw_color, 10);
	}
}

void Esp::RenderPlayerTracker(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate, bool use_vis) {
	if (player.bone_list.empty())
		return;

	auto head_bone = player.bone_list[bone_index::head];

	Vec2_t head;
	if (!matrix.wts(head_bone.pos, render_size, head))
		return;

	auto width = bounds.second.x - bounds.first.x;
	auto color = mate ? cfg::esp::colors::tracker_team : cfg::esp::colors::tracker_enemy;
	if (use_vis)
		color = mate ? cfg::esp::colors::los_visible_team : cfg::esp::colors::los_visible_enemy;

	d->AddCircle(
		head,
		width / 6,
		ImColor(color),
		15,
		cfg::esp::tracker_thickness
	);
}

void Esp::RenderPlayerBars(Player player, std::pair<Vec2_t, Vec2_t> bounds) {
	if (cfg::esp::health) {
		auto x_start = bounds.first.x - 4;
		auto x_end = x_start - 2;

		auto y_start = bounds.first.y;
		auto y_end = bounds.second.y;

		float height = y_end - y_start;
		float filled_height = height * (player.health / 100.0f);

		d->AddRectFilled(
			ImVec2(x_start, y_end - filled_height),
			ImVec2(x_end, y_end),
			IM_COL32(100, 255, 100, 255)
		);

		d->AddRect(
			ImVec2(x_start, y_start),
			ImVec2(x_end, y_end),
			IM_COL32(0, 0, 0, 50)
		);

		if (cfg::esp::health_number && player.health < 100) {
			char txt[8]{};
			snprintf(txt, sizeof(txt), "%d", player.health);
			auto sz = ImGui::CalcTextSize(txt);

			d->AddText(
				Vec2_t(
					(x_start + x_end) * 0.5f - sz.x * 0.5f,
					y_end - filled_height - sz.y * 0.5f
				),
				IM_COL32(255, 255, 255, 255),
				txt
			);
		}
	}

	if (cfg::esp::armor) {
		auto y_start = bounds.second.y + 4;
		auto y_end = y_start + 2;

		auto x_start = bounds.first.x;
		auto x_end = bounds.second.x;

		float width = x_end - x_start;
		float filled_width = width * (player.armor / 100.0f);

		d->AddRectFilled(
			ImVec2(x_start, y_start),
			ImVec2(x_start + filled_width, y_end),
			IM_COL32(150, 150, 255, 255)
		);

		d->AddRect(
			ImVec2(x_start, y_start),
			ImVec2(x_end, y_end),
			IM_COL32(0, 0, 0, 50)
		);
	}
}

void Esp::RenderPlayerFalgs(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate) {
	if (cfg::esp::flags::name) {
		char display_name[48]{};
		if (player.bot)
			snprintf(display_name, sizeof(display_name), "%s (Bot)", player.name);
		else
			snprintf(display_name, sizeof(display_name), "%s", player.name);
		auto name_size = ImGui::CalcTextSize(display_name);

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - name_size.x / 2,
				bounds.first.y - 20
			), 
			IM_COL32(255, 255, 255, 255),
			display_name
		);
	}

	if (cfg::esp::flags::ammo && player.ammo != -1) {
		char txt[8]{};
		snprintf(txt, sizeof(txt), "%d", player.ammo);
		auto ammo_size = ImGui::CalcTextSize(txt);

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - ammo_size.x / 2,
				bounds.second.y + 20
			),
			IM_COL32(255, 255, 255, 255),
			txt
		);
	}

	int offset = 0;
	static int offset_mult = 15;

	if (cfg::esp::flags::money && player.money) {
		char money_text[16]{};
		snprintf(money_text, sizeof(money_text), "%d$", player.money);
		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			IM_COL32(255, 255, 255, 255),
			money_text
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::ping && player.ping) {
		char ping_text[16]{};
		snprintf(ping_text, sizeof(ping_text), "%dms", player.ping);
		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			IM_COL32(255, 255, 255, 255),
			ping_text
		);

		offset -= offset_mult;
	}

	ImGui::PushFont(this->font_merged_icons);

	if (cfg::esp::flags::flashed && player.flashed || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::flashed_team : cfg::esp::colors::flags::flashed_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			Icons::BLIND
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::reloading && player.is_reloading || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::reloading_team : cfg::esp::colors::flags::reloading_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			Icons::RELOAD
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::defusing && player.defusing || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::defusing_team : cfg::esp::colors::flags::defusing_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			WeaponIcons::CUTTERS
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::scoped && player.scoped || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::scoped_team : cfg::esp::colors::flags::scoped_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			WeaponIcons::SCOPE
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::weapon) {
		auto weapon_size = ImGui::CalcTextSize(player.weapon.icon);

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - weapon_size.x / 2,
				bounds.second.y + 6
			),
			IM_COL32(255, 255, 255, 255),
			player.weapon.icon
		);
	}

	if (cfg::esp::flags::has_c4 && player.has_c4 || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::c4_team : cfg::esp::colors::flags::c4_enemy;

		ImGui::PushFont(this->font_merged_icons);
		auto icon_size = ImGui::CalcTextSize(WeaponIcons::C4);
		ImGui::PopFont();

		// Flash the icon if they're holding the C4, presumably planting since nobody just holds it really
		ImColor draw_color = ImColor(color);
		if (player.weapon.item_index == weapon_c4) {
			float alpha = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 8.0f);
			draw_color = ImColor(color.r, color.g, color.b, alpha);
		}

		d->AddText(
			this->font_merged_icons,
			16.0f,
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - icon_size.x / 2,
				bounds.first.y - 20 - icon_size.y - 2
			),
			draw_color,
			WeaponIcons::C4
		);

		offset -= offset_mult;
	}

	ImGui::PopFont();
}

void Esp::RenderBombBox(Bomb bomb) {
	if (!cfg::esp::bomb)
		return;

	if (!bomb.is_planted)
		return;

	float w = 10.f, l = 5.f, h = 10.f;
	Vec3_t half_size = { w / 2.f, h / 2.f, l / 2.f };

	Vec3_t corners[8] = {
		{ bomb.pos.x - half_size.x, bomb.pos.y - half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y - half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y - half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y - half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y + half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y + half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y + half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y + half_size.y, bomb.pos.z + half_size.z },
	};

	Vec2_t projected[8];
	bool visible[8] = { false };
	int visible_count = 0;

	for (int i = 0; i < 8; ++i) {
		if (matrix.wts(corners[i], render_size, projected[i])) {
			visible[i] = true;
			visible_count++;
		}
	}

	if (visible_count == 0)
		return;

	auto color = cfg::esp::colors::bomb;

	int edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // Bottom
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // Top
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }  // Verticals
	};

	for (auto& edge : edges) {
		int i = edge[0];
		int j = edge[1];
		if (visible[i] && visible[j]) {
			d->AddLine(projected[i], projected[j], ImColor(color), cfg::esp::bomb_thickness);
		}
	}

	Vec2_t screen;
	if (!matrix.wts(bomb.pos + Vec3_t(0, 0, 8), render_size, screen))
		return;

	ImGui::PushFont(this->font_merged_icons);
	d->AddText(
		this->font_merged_icons,
		16.0f,
		Vec2_t(
			screen.x - 8, // lazy
			screen.y
		),
		ImColor(255, 255, 255),
		WeaponIcons::C4
	);
	ImGui::PopFont();
}

void Esp::RenderCrosshair(Player local)
{
	if (!cfg::world::crosshair::enabled)
		return;

	if (local.scoped)
		return;

	auto weapon = local.weapon;

	if (weapon.item_index == -1)
		return;

	switch (weapon.item_index) {
	case weapon_ssg08:
	case weapon_awp:
	case weapon_g3sg1:
	case weapon_scar20:
		break;
	default:
		return;
	}

	ImVec2 center(
		floorf(render_size.x * 0.5f),
		floorf(render_size.y * 0.5f));

	constexpr float size = 6.f;
	constexpr float thickness = 1.0f;

	d->AddLine(
		ImVec2(center.x - size, center.y),
		ImVec2(center.x + size + 1, center.y),
		IM_COL32(255, 255, 255, 255),
		thickness);
	d->AddLine(
		ImVec2(center.x, center.y - size),
		ImVec2(center.x, center.y + size + 1),
		IM_COL32(255, 255, 255, 255),
		thickness);
}

void Esp::RenderPlayerBox3D(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate, bool use_vis) {
	float height = 72.0f;
	float width = 32.0f;
	float depth = 32.0f;

	Vec3_t base = player.pos;
	Vec3_t top = base + Vec3_t(0, 0, height);

	Vec3_t corners[8] = {
		{ base.x - width / 2, base.y - depth / 2, base.z },
		{ base.x + width / 2, base.y - depth / 2, base.z },
		{ base.x + width / 2, base.y + depth / 2, base.z },
		{ base.x - width / 2, base.y + depth / 2, base.z },
		{ top.x - width / 2, top.y - depth / 2, top.z },
		{ top.x + width / 2, top.y - depth / 2, top.z },
		{ top.x + width / 2, top.y + depth / 2, top.z },
		{ top.x - width / 2, top.y + depth / 2, top.z },
	};

	Vec2_t projected[8];
	for (int i = 0; i < 8; ++i) {
		if (!matrix.wts(corners[i], render_size, projected[i]))
			return;
	}

	auto color = mate ? cfg::esp::colors::box_team : cfg::esp::colors::box_enemy;
	if (use_vis)
		color = mate ? cfg::esp::colors::los_visible_team : cfg::esp::colors::los_visible_enemy;
	ImColor col(color);
	float thickness = cfg::esp::box_3d_thickness;

	d->AddLine(projected[0], projected[1], col, thickness);
	d->AddLine(projected[1], projected[2], col, thickness);
	d->AddLine(projected[2], projected[3], col, thickness);
	d->AddLine(projected[3], projected[0], col, thickness);

	d->AddLine(projected[4], projected[5], col, thickness);
	d->AddLine(projected[5], projected[6], col, thickness);
	d->AddLine(projected[6], projected[7], col, thickness);
	d->AddLine(projected[7], projected[4], col, thickness);

	d->AddLine(projected[0], projected[4], col, thickness);
	d->AddLine(projected[1], projected[5], col, thickness);
	d->AddLine(projected[2], projected[6], col, thickness);
	d->AddLine(projected[3], projected[7], col, thickness);
}

void Esp::RenderPlayerTracers(Player source, Player player, bool mate) {
	if (!cfg::esp::tracers)
		return;

	Vec2_t screenPos;
	bool projected = matrix.wts(player.pos, render_size, screenPos, false);

	if (!projected)
	{
		Vec3_t camPos = source.pos;
		Vec3_t dir = player.pos - camPos;

		// projection for off screen players
		Vec3_t viewDir;
		viewDir.x = matrix[0][0] * dir.x + matrix[0][1] * dir.y + matrix[0][2] * dir.z;
		viewDir.y = matrix[1][0] * dir.x + matrix[1][1] * dir.y + matrix[1][2] * dir.z;
		viewDir.z = matrix[2][0] * dir.x + matrix[2][1] * dir.y + matrix[2][2] * dir.z;

		if (viewDir.z > 0.0f)
		{
			viewDir.x = -viewDir.x;
			viewDir.y = -viewDir.y;
		}

		// normalize
		float len = sqrt(viewDir.x * viewDir.x + viewDir.y * viewDir.y);
		if (len > 0.001f)
		{
			viewDir.x /= len;
			viewDir.y /= len;
		}

		screenPos.x = render_size.x * 0.5f + viewDir.x * render_size.x * 0.5f;
		screenPos.y = render_size.y * 0.5f - viewDir.y * render_size.y * 0.5f;

		float margin = 10.f;
		screenPos.x = std::clamp(screenPos.x, margin, render_size.x - margin);
		screenPos.y = std::clamp(screenPos.y, margin, render_size.y - margin);
	}

	auto color = mate ? cfg::esp::colors::tracer_team : cfg::esp::colors::tracer_enemy;

	d->AddLine(
		Vec2_t(render_size.x * 0.5f, render_size.y * 0.5f),
		screenPos,
		ImColor(color),
		cfg::esp::tracer_thickness
	);
}

void Esp::RenderMapWireframe(Player local) {
	if (!cfg::world::map_wireframe::enabled)
		return;
	if (!local.alive)
		return;

	auto triangles = VisCheckManager::GetWireframeTriangles();
	if (!triangles || triangles->empty())
		return;

	const float max_distance = std::max(128.0f, cfg::world::map_wireframe::distance);
	const float max_distance_sqr = max_distance * max_distance;
	const float thickness = std::max(0.5f, cfg::world::map_wireframe::thickness);
	const int max_edges = std::max(100, cfg::world::map_wireframe::max_edges);
	const ImColor color(cfg::world::map_wireframe::color);

	auto to_vec3 = [](const Vector3& v) {
		return Vec3_t(v.x, v.y, v.z);
	};

	int drawn_edges = 0;
	for (const auto& tri : *triangles) {
		const Vec3_t center(
			(tri.v0.x + tri.v1.x + tri.v2.x) / 3.0f,
			(tri.v0.y + tri.v1.y + tri.v2.y) / 3.0f,
			(tri.v0.z + tri.v1.z + tri.v2.z) / 3.0f
		);
		if ((center - local.pos).length_sqr() > max_distance_sqr)
			continue;

		Vec2_t p0, p1, p2;
		if (!matrix.wts(to_vec3(tri.v0), render_size, p0))
			continue;
		if (!matrix.wts(to_vec3(tri.v1), render_size, p1))
			continue;
		if (!matrix.wts(to_vec3(tri.v2), render_size, p2))
			continue;

		d->AddLine(p0, p1, color, thickness);
		if (++drawn_edges >= max_edges)
			break;
		d->AddLine(p1, p2, color, thickness);
		if (++drawn_edges >= max_edges)
			break;
		d->AddLine(p2, p0, color, thickness);
		if (++drawn_edges >= max_edges)
			break;
	}
}
