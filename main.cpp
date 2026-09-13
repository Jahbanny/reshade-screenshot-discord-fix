/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Jahbanny
 *
 * ReShade Add-on: Fix Discord HDR Screenshots
 * Automatically strips the cICP chunk from HDR PNG screenshots so Discord's
 * image viewer displays proper HDR colors using the fallback ICC profile.
 *
 * This add-on is always-on: it registers no ReShade overlay and exposes no
 * UI, so it runs in the background without appearing in the Add-ons tab.
 */

#include <reshade.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>   // std::memcmp

// Strips the 16-byte cICP chunk from a PNG file if present
static bool strip_cicp_from_png(const std::filesystem::path &filepath)
{
	// Read the entire file into memory
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file)
		return false;

	const std::streamsize size = file.tellg();
	file.seekg(0);

	std::vector<char> buffer(static_cast<std::size_t>(size));
	if (!file.read(buffer.data(), size))
		return false;

	// Find the cICP chunk (signature: 0x00000004 63 49 43 50)
	const uint32_t cicp_length = 0x00000004;
	const uint32_t cicp_type   = 0x63494350; // 'cICP'

	int cicp_index = -1;
	for (int i = 0; i + 8 < (int)size; ++i)
	{
		if (std::memcmp(&buffer[i], &cicp_length, 4) == 0 &&
			std::memcmp(&buffer[i + 4], &cicp_type, 4) == 0)
		{
			cicp_index = i;
			break;
		}
	}

	if (cicp_index == -1)
		return false;

	// Remove the 16-byte cICP chunk and rewrite the file
	std::vector<char> new_buffer;
	new_buffer.insert(new_buffer.end(), buffer.begin(), buffer.begin() + cicp_index);
	new_buffer.insert(new_buffer.end(), buffer.begin() + cicp_index + 16, buffer.end());

	std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;

	out.write(new_buffer.data(), new_buffer.size());
	return true;
}

// ReShade event handler: fires after each screenshot is saved
static void on_screenshot(reshade::api::effect_runtime *, const char *path_string)
{
	if (path_string == nullptr)
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

extern "C" __declspec(dllexport) const char *NAME = "Discord HDR Screenshot Fix";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Automatically strips the cICP chunk from HDR PNG screenshots to prevent washed-out colors in Discord.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		reshade::register_event<reshade::addon_event::reshade_screenshot>(on_screenshot);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::reshade_screenshot>(on_screenshot);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}