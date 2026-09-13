# ReShade Screenshot Discord Fix

A lightweight [ReShade](https://reshade.me) Add-on that automatically strips the `cICP` chunk from HDR PNG screenshots so Discord's built-in image viewer displays proper HDR colors using the fallback ICC profile.

---

## The Problem

When taking HDR screenshots in PNG format with ReShade, both an **ICC profile** (`iCCP`) and a **Coding-Independent Code Points** chunk (`cICP`) are written to the PNG file.

Discord currently has a bug in its Chromium/Electron-based image viewer where it misreads the `cICP` color chunk, resulting in **washed-out, desaturated, or grayed-out images** when previewed directly in chat.

## The Solution

This add-on hooks ReShade's screenshot event:
1. Detects whenever a PNG screenshot is saved.
2. Inspects the PNG chunks and removes the 16-byte `cICP` tag.
3. Leaves the embedded ICC profile intact so Discord correctly parses the image's HDR color space.

No external scripts or post-save command setups are required!

---

## Installation

1. Download the latest release from the [Releases](https://github.com/Jahbanny/reshade-screenshot-discord-fix/releases) tab.
2. Choose the correct binary for your game:
   - 64-bit games: `DiscordHDRFix.addon64`
   - 32-bit games: `DiscordHDRFix.addon32`
3. Place the `.addon64` (or `.addon32`) file into the same directory as your game's executable (next to ReShade's `.dll` or in your game's folder).
4. Launch the game. In ReShade's overlay under the **Add-ons** tab, you will see **Discord HDR Fix** enabled.

---

## Building from Source

Requirements:
- Visual Studio 2022 (with "Desktop development with C++" workload)

Build command:
```cmd
msbuild reshade-screenshot-discord-fix.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild reshade-screenshot-discord-fix.vcxproj /p:Configuration=Release /p:Platform=Win32
```

---

## License

MIT License.
