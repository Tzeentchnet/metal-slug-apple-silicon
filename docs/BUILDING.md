# Building on Apple Silicon

## Install the pinned environment

Install Apple Command Line Tools, then run:

```sh
./setup/bootstrap.sh
source setup/env.sh
```

The bootstrap downloads official, hash-checked CMake, Ninja, Python, and SDL3 artifacts into
`.tools/`. It probes installed macOS SDKs with a real arm64 compile-and-link test and records the
newest coherent SDK.

Optional analysis tools:

```sh
./setup/bootstrap.sh --analysis-tools
source setup/env.sh
python3 -I setup/build_mame.py
```

This builds a focused arm64 MAME `mslug` oracle under `.tools/mame-build/`.

## Personal native build

Put your loose MAME set in `game/mslug/` and `game/neogeo/`, then run:

```sh
cmake -S . -B build-personal \
  -DPD_RECOMPILED=ON \
  -DPD_GAME="$PWD/game" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-personal -j 8
ctest --test-dir build-personal --output-on-failure
open build-personal/src/mslug.app
```

The supported cartridge files are `201-p1.p1`, `201-s1.s1`, `201-m1.m1`, `201-v1.v1`,
`201-v2.v2`, and `201-c1.c1` through `201-c4.c4`. The loader verifies them by CRC-32 and SHA-1,
then selects a verified 128 KiB MVS BIOS from `game/neogeo/`.

Private outputs live under `build-personal/private/`. The default static analysis generated 25,168
instructions in 1,081 native regions for the verified set. To add optional dynamic coverage:

```sh
python3 -I tools/coverage.py \
  --mame "$PD_MAME" \
  --rompath "$PD_GAME" \
  --work build-personal/private/mame-coverage \
  --output build-personal/private/analysis/coverage.json \
  --seconds 180

cmake -S . -B build-personal -DPD_RECOMPILED=ON -DPD_GAME="$PD_GAME"
cmake --build build-personal -j 8
```

## Clean build

```sh
cmake -S . -B build-dist \
  -DPD_RECOMPILED=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-dist -j 8
```

Only this build may be packaged for distribution.

## Benchmarks

```sh
python3 -I tools/benchmark.py \
  --app build-personal/src/mslug.app/Contents/MacOS/mslug \
  --roms game \
  --frames 1800 \
  --window-seconds 10 \
  --output build-personal/benchmark.json
```

Use `PD_CAPABILITIES=1` to print live CPU and Metal capabilities. Use Instruments with subsystem
`io.github.mslug.recomp` and category `frame` for signposts.
