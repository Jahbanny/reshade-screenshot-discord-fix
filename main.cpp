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

// Strips the cICP chunk from a PNG file if present.
// Walks the PNG chunk chain (4-byte length + 4-byte type + data + 4-byte CRC)
// and copies every chunk except the cICP one, so the result is still a valid PNG.
static bool strip_cicp_from_png(const std::filesystem::path &filepath)
{
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// Verify the 8-byte PNG signature: 137, 80, 78, 71, 13, 10, 26, 10
	static const uint8_t png_sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (data.size() < 8 || std::memcmp(data.data(), png_sig, 8) != 0)
		return false;

	std::vector<uint8_t> new_data;
	new_data.reserve(data.size());
	new_data.insert(new_data.end(), data.begin(), data.begin() + 8); // keep the PNG signature

	bool removed = false;
	std::size_t offset = 8;
	while (offset + 12 <= data.size())
	{
		const uint32_t chunk_len = (static_cast<uint32_t>(data[offset]) << 24) |
			(static_cast<uint32_t>(data[offset + 1]) << 16) |
			(static_cast<uint32_t>(data[offset + 2]) << 8) |
			static_cast<uint32_t>(data[offset + 3]);
		const std::size_t chunk_total = 12 + chunk_len; // length + type + data + crc

		if (offset + chunk_total > data.size())
			break; // malformed or truncated chunk; stop here

		const bool is_cicp = std::memcmp(&data[offset + 4], "cICP", 4) == 0;
		if (is_cicp)
			removed = true;
		else
			new_data.insert(new_data.end(), data.begin() + offset, data.begin() + offset + chunk_total);

		if (std::memcmp(&data[offset + 4], "IEND", 4) == 0)
			break;

		offset += chunk_total;
	}

	if (!removed)
		return false; // no cICP chunk; leave the file untouched

	std::ofstream out_file(filepath, std::ios::binary | std::ios::trunc);
	if (!out_file.is_open())
		return false;
	out_file.write(reinterpret_cast<const char *>(new_data.data()), static_cast<std::streamsize>(new_data.size()));
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