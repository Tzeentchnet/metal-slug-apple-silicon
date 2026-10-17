# Verification

## Verified input set

The loader accepts the original Metal Slug MAME set (`mslug`) and a verified 128 KiB Neo Geo MVS
BIOS. It identifies files by CRC-32 and SHA-1 rather than trusting file names. The supplied loose
set was also packed into `mslug.zip` and `neogeo.zip` during testing; both layouts normalized to
identical program, BIOS, fixed-layer, sprite, Z80, and ADPCM data.

## Native program generation

The personal build:

- validated `201-p1.p1` and the US MVS BIOS;
- normalized the reversed 1 MiB P1 halves into 68000 address order;
- generated 25,168 instructions in 1,081 native regions from 1,442 analysis entries;
- reported zero unsupported instructions;
- selected the generated program only when the loaded P1 and BIOS hashes matched.

A 600-frame headless run confirmed that the recompiled path was active:

```text
recompiled
frames: 600
recompiled steps: 94517
interpreted: 6868487
unmapped reads: 0
unmapped writes: 0
watchdog resets: 0
```

The interpreted count is expected: ungenerated addresses deliberately fall back to Musashi.

## Automated tests

The personal arm64 build currently registers and passes eight tests:

| Test | Coverage |
|---|---|
| `mac_capabilities` | Apple Silicon and Metal capability classification |
| `verified_rom_set` | Exact cartridge/BIOS validation and startup |
| `roms_mame_folder` | Loose-folder loader and normalized-data comparison |
| `roms_mame_zips_make` | Test ZIP creation |
| `roms_mame_zip` | ZIP loader parity with the loose set |
| `savestate_roundtrip` | RAM, image, and audio determinism after restore |
| `cpu_differential` | Generated instruction behavior against Musashi |
| `lockstep_reset` | Whole-program generated/interpreter reset lockstep |

Run the matrix with:

```sh
source setup/env.sh
ctest --test-dir build-personal --output-on-failure
```

The clean interpreter build also passes the platform and loader tests. A separate 600-frame clean
run completed with zero unmapped accesses and zero watchdog resets.

A native-window test launched without `--roms`, found the repository's `game/` directory through
the development-bundle search path, rendered through `CAMetalLayer`, and completed a 1792x1344 PNG
readback before exiting normally.

## Release checks

The release packager:

- rejects any build configured with `PD_RECOMPILED=ON`;
- thins the app executable and embedded SDL3 framework to arm64;
- strips and signs nested code in the correct order;
- writes a machine-readable ROM-free build manifest;
- audits the ZIP allowlist and recursively scans for byte runs copied from the owner's ROM files.

The generated `metal-slug-recomp-v1.0.0-macos-arm64.zip` passed the release audit with ROM scanning
enabled. The ZIP is a local validation artifact, not tracked source.

## Current limits

The core loader, CPU paths, Neo Geo runtime, audio/video pipeline, save states, and native frontend
are validated. A Metal Slug-specific `GameProfile` has not yet been established, so direct
game-memory enhancements, named sound metadata, automatic rumble mappings, and title-logo
extraction stay disabled. Full mission-by-mission parity against MAME has not yet been recorded.
