# Metal Slug Apple Silicon recompilation

A native macOS 13+ port of **Metal Slug** for Apple Silicon. The Neo Geo 68000 program can be
privately recompiled to arm64 C from a ROM set you own, while SDL3 provides lifecycle, input, and
CoreAudio integration and a dedicated Metal backend renders the full frontend.

Tracked source and clean release artifacts contain no ROM, BIOS, game graphics, game audio, or
generated game code. Metal Slug is copyright SNK CORPORATION. This is an unofficial fan project
and is not affiliated with or endorsed by SNK.

This port is adapted from
[Tzeentchnet/metal-slug-x-apple-silicon](https://github.com/Tzeentchnet/metal-slug-x-apple-silicon),
which in turn was inspired by
[AlmazProud/metal-slug-x-recomp](https://github.com/AlmazProud/metal-slug-x-recomp).

## Requirements

- Apple Silicon Mac (M1 or newer)
- macOS 13 Ventura or newer
- Apple Command Line Tools
- Your own verified Metal Slug MAME set:
  - `game/mslug/` and `game/neogeo/`, or
  - `mslug.zip` and `neogeo.zip`

Only the verified MAME arcade set is supported.

## Quick start

```sh
./setup/bootstrap.sh
source setup/env.sh
cmake -S . -B build-personal \
  -DPD_RECOMPILED=ON \
  -DPD_GAME="$PWD/game" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-personal -j 8
open "build-personal/src/mslug.app"
```

The personal build verifies and normalizes your P1 and US BIOS in the ignored build directory,
runs static analysis, and generates native code there. Unknown program or BIOS revisions continue
through the cycle-accurate interpreter.

For a ROM-free distributable build:

```sh
cmake -S . -B build-dist -DPD_RECOMPILED=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dist -j 8
python3 -I tools/package_release.py --build build-dist --out release
```

The packager refuses a build containing generated game code and creates an arm64-only `.app`, ZIP,
and DMG. Developer ID signing and notarization are optional; see
[Releasing](docs/RELEASING.md).

## Native features

- Exact Neo Geo cadence: 202,752 cycles at 12 MHz, or **59.185606 Hz**
- Display-independent 60 Hz and ProMotion presentation without changing game speed
- Direct CAMetalLayer renderer for game pixels, menus, filters, scanlines, overlays, and captures
- Retina output with square-pixel, 4:3, nearest, smooth, and sharp scaling
- SDL3 keyboard, remappable two-player gamepads, and CoreAudio device support
- Save states, autofire, filters, scanlines, screenshots, and hardware debugging views
- An M1-compatible arm64 app with runtime Metal and hardware capability reporting
- Private static recompilation with interpreter fallback at ungenerated addresses

Revision-specific cheats, automatic rumble mappings, the named sound catalog, and title-logo
extraction remain disabled until their 201-P1 addresses and data have been independently verified.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | Arrow keys | D-pad / left stick |
| Shoot / jump / grenade | Z / X / C | West / South / East |
| Start / coin | 1 / 5 | Start / Back |
| Menu | Esc | Guide or Back+Start |
| Save / load | F5 / F9 | Menu |
| Fullscreen | F11 or Control-Command-F | Menu setting |
| Pause | P | Menu |
| Service button | F2 | - |
| Debug panel | F3 | - |

Controls are remappable in the app. Settings and personal data are stored under
`~/Library/Application Support/mslug/Metal Slug/`.

## Verification

The supplied `201-*` set builds into 25,168 generated instructions across 1,081 native regions with
zero unsupported instructions. The native personal build passes ROM-loader parity, save-state,
instruction differential, whole-program reset lockstep, and macOS capability tests on arm64.

See:

- [Architecture](docs/ARCHITECTURE.md)
- [Building](docs/BUILDING.md)
- [Verification](docs/VERIFICATION.md)
- [Translation and conversion findings](docs/TRANSLATION_FINDINGS.md)
- [Releasing](docs/RELEASING.md)

## Repository layout

| Path | Purpose |
|---|---|
| `src/cpu/` | generated-code dispatcher and Musashi fallback |
| `src/neogeo/` | Neo Geo bus, video, sound, RTC, scheduler, ROM loader |
| `src/port/` | SDL3 app lifecycle and direct Metal frontend |
| `tools/recomp68k/` | private 68000-to-C generator |
| `tools/mame/` | MAME debugger automation used as an oracle |
| `tests/` | instruction differential, lockstep, loader, and capability tests |
| `setup/` | pinned, hash-verified Apple Silicon toolchain |

## License

The port code is MIT licensed under [LICENSE](LICENSE). Third-party components retain their own
licenses; see [THIRD_PARTY.md](THIRD_PARTY.md).
