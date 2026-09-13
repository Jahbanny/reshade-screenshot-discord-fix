/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Jahbanny
 *
 * ReShade Add-on: Fix Discord HDR Screenshots
 * Automatically strips the cICP chunk from HDR PNG screenshots so Discord's
 * image viewer displays proper HDR colors using the fallback ICC profile.
 */

#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

static bool s_enabled = true;

// Strips the 16-byte cICP chunk from a PNG file if present
static bool strip_cicp_from_png(const std::filesystem::path &filepath)
{
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// Check PNG signature: 137, 80, 78, 71, 13, 10, 26, 10
	static const uint8_t png_sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (data.size() < 8 || std::memcmp(data.data(), png_sig, 8) != 0)
		return false;

	// Search for 'cICP' chunk (0x63, 0x49, 0x43, 0x50)
	size_t offset = 8;
	while (offset + 12 <= data.size())
	{
		const uint32_t chunk_len = (static_cast<uint32_t>(data[offset]) << 24) |
		                           (static_cast<uint32_t>(data[offset + 1]) << 16) |
		                           (static_cast<uint32_t>(data[offset + 2]) << 8) |
		                            static_cast<uint32_t>(data[offset + 3]);

		const uint8_t *chunk_type = &data[offset + 4];

		if (std::memcmp(chunk_type, "cICP", 4) == 0)
		{
			// Found cICP chunk! Chunk total size = 4 (length) + 4 (type) + chunk_len (data) + 4 (crc)
			const size_t total_chunk_len = 12 + chunk_len;
			if (offset + total_chunk_len <= data.size())
			{
				data.erase(data.begin() + offset, data.begin() + offset + total_chunk_len);

				std::ofstream out_file(filepath, std::ios::binary | std::ios::trunc);
				if (out_file.is_open())
				{
					out_file.write(reinterpret_cast<const char *>(data.data()), data.size());
					return true;
				}
			}
			break;
		}

		if (std::memcmp(chunk_type, "IEND", 4) == 0)
			break;

		offset += 12 + chunk_len;
	}

	return false;
}

static void on_screenshot(reshade::api::effect_runtime *, const char *path_string)
{
	if (!s_enabled || path_string == nullptr)
		return;

	const std::filesystem::path path = std::filesystem::u8path(path_string);
	if (path.extension() == ".png" || path.extension() == ".PNG")
	{
		if (strip_cicp_from_png(path))
		{
			reshade::log::message(reshade::log::level::info, "Stripped cICP chunk from HDR PNG screenshot for Discord compatibility.");
		}
	}
}

static void draw_overlay(reshade::api::effect_runtime *runtime)
{
	if (ImGui::Checkbox("Strip cICP from HDR PNGs (Discord Fix)", &s_enabled))
	{
		reshade::set_config_value(runtime, "HDR_DISCORD_FIX", "Enabled", s_enabled);
	}

	ImGui::SetItemTooltip(
		"Strips the cICP color chunk from HDR PNG screenshots.\n"
		"Discord currently has a bug where it misinterprets cICP tags, causing washed-out screenshots.\n"
		"Removing cICP allows Discord to fall back to the embedded ICC profile for accurate colors.");

	ImGui::Spacing();
	ImGui::TextDisabled("Status: %s", s_enabled ? "Active (cICP will be stripped automatically)" : "Disabled");
}

extern "C" __declspec(dllexport) const char *NAME = "Discord HDR Screenshot Fix";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Automatically strips the cICP chunk from HDR PNG screenshots to prevent washed-out colors in Discord.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::get_config_value(nullptr, "HDR_DISCORD_FIX", "Enabled", s_enabled);

		reshade::register_event<reshade::addon_event::reshade_screenshot>(on_screenshot);
		reshade::register_overlay("Discord HDR Fix", draw_overlay);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_overlay("Discord HDR Fix", draw_overlay);
		reshade::unregister_event<reshade::addon_event::reshade_screenshot>(on_screenshot);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
