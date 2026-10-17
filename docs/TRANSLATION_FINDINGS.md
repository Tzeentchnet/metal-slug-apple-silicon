# Translation and conversion findings

This document records Metal Slug-specific findings from adapting the Apple Silicon runtime. It
contains public ROM hashes and aggregate analysis results, but no ROM bytes or generated game code.

## Host and toolchain

- Target: Apple Silicon, macOS 13 or newer.
- The installed macOS 27 SDK contains TAPI architecture declarations unsupported by the selected
  linker. The bootstrap's compile-and-link probe selected the coherent `MacOSX26.sdk` instead.
- SDL3 3.4.18 is supplied as a universal framework. Development apps use its arm64 slice; clean
  packaging thins the framework and executable to arm64-only.
- Command Line Tools do not include `xcrun metal`, so the renderer compiles its small Metal shader
  source at startup and reports compilation failures directly.

## Verified arcade inputs

The cartridge files match MAME's original Metal Slug (`NGM-201`) definition:

| Input | Size | CRC-32 | SHA-1 |
|---|---:|---:|---|
| `201-p1.p1` | 2 MiB | `08d8daa5` | `b888993dbb7e9f0a28a01d7d2e1da00ef9cf6f38` |
| `201-s1.s1` | 128 KiB | `2f55958d` | `550b53628daec9f1e1e11a398854092d90f9505a` |
| `201-m1.m1` | 128 KiB | `c28b3253` | `fd75bd15aed30266a8b3775f276f997af57d1c06` |
| `201-v1.v1` | 4 MiB | `23d22ed1` | `cd076928468ad6bcc5f19f88cb843ecb5e660681` |
| `201-v2.v2` | 4 MiB | `472cf9db` | `5f79ea9286d22ed208128f9c31ca75552ce08b57` |
| `201-c1.c1` | 4 MiB | `72813676` | `7b045d1a48980cb1a140699011cb1a3d4acdc4d1` |
| `201-c2.c2` | 4 MiB | `96f62574` | `cb7254b885989223bba597b8ff0972dfa5957816` |
| `201-c3.c3` | 4 MiB | `5121456a` | `0a7a27d603d1bb2520b5570ebf5b34a106e255a6` |
| `201-c4.c4` | 4 MiB | `f4ad59a3` | `4e94fda8ee63abf0f92afe08060a488546e5c280` |

The selected US MVS BIOS is `sp-u2.sp1` (CRC-32 `e72943de`, SHA-1
`5c6bba07d2ec8ac95776aa3511109f5e1e2e92eb`).

## Program-ROM normalization

`201-p1.p1` is not linearly mapped as stored. MAME loads the file's first 1 MiB at CPU address
`0x100000`, then continues the second 1 MiB at CPU address `0x000000`. The loader therefore:

1. verifies the original 2 MiB file as a whole;
2. copies the second file half to normalized offset zero;
3. copies the first file half to normalized offset `0x100000`;
4. swaps each word into the big-endian byte order seen by the 68000.

The private preparation tool reverses only the per-word byte swap before analysis. It preserves
CPU-address half order, so the analyzer and generator see vectors and executable code at offset
zero. Runtime and generated-program hash checks use the same normalized first-megabyte boundary.

## Cartridge mapping

The Metal Slug X reference runtime included a cartridge-specific protection window at
`0x2fffe0`-`0x2fffef`. Original Metal Slug uses the standard second-megabyte program window and no
such protection device. The adapted bus maps the normalized second P1 megabyte directly at
`0x200000` and leaves the standard bank register behavior intact. Headless verification observed
zero unmapped reads and writes.

## Static recompilation

The verified personal build generated:

- 25,168 68000 instructions;
- 1,081 native regions;
- 1,442 static analysis entries;
- zero unsupported instructions.

Only the directly mapped program megabyte and BIOS are registered as generated regions. Any
unclassified address, including code not reached by static analysis, executes through the shared
Musashi state and returns to generated code at the next registered region.

## Revision-specific enhancements

The reference project's feature addresses, title sprites, sound names, and rumble table belong to
a different game program. Reusing them would risk invalid RAM writes and incorrect UI metadata.
The Metal Slug `GameProfile` therefore selects no revision. Hardware emulation and normal frontend
features remain active, while the following stay gated off:

- developer/cheat writes and routine calls;
- forced red-blood configuration;
- title-logo sprite extraction;
- built-in sound names and automatic rumble strengths.

Recent in-game sound commands and user-assigned command names remain available without assuming a
game-specific table.

## Distribution boundary

Normalized programs, analysis listings, and generated native C remain under ignored
`build-personal/private/` and build-tree directories. The distributable app is built separately
with `PD_RECOMPILED=OFF`, contains only the interpreter and project runtime, and asks the user for
their own verified set at launch.
