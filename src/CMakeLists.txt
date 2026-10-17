# Port core: CPU (dispatcher + fallback), recompiled program and Neo Geo runtime.

add_library(cpu68k STATIC cpu/dispatcher.c cpu/musashi_interpreter.c)
target_include_directories(cpu68k PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cpu)
target_link_libraries(cpu68k PUBLIC musashi)

# Musashi's base 68000 cycle table, used by the generator.
add_executable(dump_cycles ${PROJECT_SOURCE_DIR}/tools/recomp68k/dump_cycles.c)
target_link_libraries(dump_cycles PRIVATE musashi)

# Query for the interrupt sampling rules (tools/verify_sampling.py).
add_executable(sampling_classes ${PROJECT_SOURCE_DIR}/tools/recomp68k/sampling_classes.c)
target_link_libraries(sampling_classes PRIVATE cpu68k)

# Hash-verified ROM loader, also used by the private recompiler-input preparation tool.
add_library(romloader STATIC neogeo/roms.c neogeo/hashes.c neogeo/sha256.c neogeo/unzip.c)
target_include_directories(romloader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/neogeo
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu)
target_compile_options(romloader PRIVATE -Wall -Wextra)

add_executable(prepare_recomp ${PROJECT_SOURCE_DIR}/tools/prepare_recomp.c)
target_link_libraries(prepare_recomp PRIVATE romloader)

# The recompiled program is generated from the user's copy and private analysis listings.
# PD_RECOMPILED=OFF is the only clean, distributable mode. A requested personal build fails on
# missing or invalid input instead of silently producing an interpreter build.
option(PD_RECOMPILED "Build the recompiled program from the user's copy (personal builds)" ON)
set(HAVE_PROGRAM ${PD_RECOMPILED})

set(PD_HAVE_PROGRAM ${HAVE_PROGRAM} CACHE INTERNAL "The recompiled program is built")

if(HAVE_PROGRAM)
set(NORMALIZED_P ${PD_ROMS}/mslug_game_m68k)
set(NORMALIZED_BIOS ${PD_ROMS}/mslug_bios_m68k)
set(ROM_MANIFEST ${PD_ROMS}/manifest.json)
add_custom_command(
    OUTPUT ${NORMALIZED_P} ${NORMALIZED_BIOS} ${ROM_MANIFEST}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PD_ROMS}
    COMMAND prepare_recomp --input ${PD_GAME} --output ${PD_ROMS}
            --require-version arcade --require-bios us
    DEPENDS prepare_recomp
    COMMENT "Preparing private arcade P1 and US BIOS inputs"
    VERBATIM)
add_custom_target(recomp_inputs DEPENDS ${NORMALIZED_P} ${NORMALIZED_BIOS} ${ROM_MANIFEST})

set(LISTING_P1 ${PD_ANALYSIS}/listing_p1.txt)
set(LISTING_BIOS ${PD_ANALYSIS}/listing_bios.txt)
set(COVERAGE_JSON ${PD_ANALYSIS}/coverage.json)
set(ANALYSIS_DEPENDS ${PROJECT_SOURCE_DIR}/tools/analyze68k.py ${NORMALIZED_P} ${NORMALIZED_BIOS})
if(EXISTS "${COVERAGE_JSON}")
    list(APPEND ANALYSIS_DEPENDS ${COVERAGE_JSON})
endif()
add_custom_command(
    OUTPUT ${LISTING_P1} ${LISTING_BIOS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PD_ANALYSIS}
    COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tools/analyze68k.py
            --roms ${PD_ROMS} --coverage ${COVERAGE_JSON}
            --output ${PD_ANALYSIS}
    DEPENDS ${ANALYSIS_DEPENDS}
    COMMENT "Analyzing the private arcade 68000 program"
    VERBATIM)
add_custom_target(recomp_analysis DEPENDS ${LISTING_P1} ${LISTING_BIOS})

set(CYCLES_BIN ${CMAKE_CURRENT_BINARY_DIR}/cycles_68000.bin)
add_custom_command(OUTPUT ${CYCLES_BIN} COMMAND dump_cycles ${CYCLES_BIN} DEPENDS dump_cycles VERBATIM)

set(PROG_DIR ${CMAKE_CURRENT_BINARY_DIR}/program)
set(PROG_C ${PROG_DIR}/program_registry.c)
foreach(n RANGE 0 15)
    string(LENGTH "${n}" len)
    if(len EQUAL 1)
        set(num "00${n}")
    else()
        set(num "0${n}")
    endif()
    list(APPEND PROG_C ${PROG_DIR}/program_${num}.c)
endforeach()

set(COVERAGE ${COVERAGE_JSON})
if(EXISTS "${PD_ANALYSIS}/coverage_mission1.json")
    list(APPEND COVERAGE "${PD_ANALYSIS}/coverage_mission1.json")
endif()

file(GLOB RECOMP_PY ${PROJECT_SOURCE_DIR}/tools/recomp68k/*.py)
add_custom_command(
    OUTPUT ${PROG_C}
    COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tools/recomp68k/program.py
            --roms ${PD_ROMS}
            --listings ${LISTING_P1} ${LISTING_BIOS}
            --coverage ${COVERAGE}
            --cycles ${CYCLES_BIN} --output ${PROG_DIR} --files 16
    DEPENDS ${RECOMP_PY} ${CYCLES_BIN} ${LISTING_P1} ${LISTING_BIOS}
    COMMENT "Recompiling the user's P1 and BIOS to C"
    VERBATIM)

# Code generated from the user's copy: it lives only in build/.
add_library(recompiled_program STATIC ${PROG_C})
target_link_libraries(recompiled_program PUBLIC cpu68k)
target_compile_options(recompiled_program PRIVATE -O1 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label)
else()
    message(STATUS "Interpreter-only build (no code derived from the game)")
    add_library(recompiled_program STATIC cpu/program_none.c)
    target_link_libraries(recompiled_program PUBLIC cpu68k)
endif()

# Neo Geo hardware runtime (Phase 3).
add_library(neogeo STATIC
    neogeo/benchmark.c neogeo/bus.c neogeo/savestate.c neogeo/machine.c neogeo/rtc.c
    neogeo/sound.c neogeo/video.c neogeo/z80_impl.c neogeo/ym2610.cpp)
target_include_directories(neogeo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/neogeo
                           PRIVATE ${PROJECT_SOURCE_DIR}/external/chips)
target_link_libraries(neogeo PUBLIC cpu68k ymfm romloader)
set_target_properties(neogeo PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(neogeo PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>)
# z80.h (third party) produces its own warnings; it is compiled without them.
set_source_files_properties(neogeo/z80_impl.c PROPERTIES COMPILE_OPTIONS "-w")

# Port application (SDL3 lifecycle/input/audio and direct Metal presentation).
find_package(SDL3 CONFIG REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
set(PORT_SOURCES port/main.c port/headless.c port/window.c port/config.c port/debug.c port/font.c
    port/find_game.c port/lang.c port/profile.c port/cheats.c port/logo.c port/menu.c port/sounds.c
    port/profile_sounds.c port/metal_renderer.mm port/mac_capabilities.mm)
add_executable(mslug MACOSX_BUNDLE ${PORT_SOURCES})
target_link_libraries(mslug PRIVATE neogeo recompiled_program SDL3::SDL3
    ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK} ${CORETEXT_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK})
target_compile_options(mslug PRIVATE -Wall -Wextra)
set_source_files_properties(port/metal_renderer.mm port/mac_capabilities.mm
                            PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
set_target_properties(mslug PROPERTIES LINKER_LANGUAGE CXX)

file(STRINGS port/version.h PORT_VERSION_LINE REGEX "^#define PORT_VERSION ")
string(REGEX REPLACE "^#define PORT_VERSION \"([^\"]+)\".*" "\\1" PORT_VERSION "${PORT_VERSION_LINE}")
configure_file(port/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
set_target_properties(mslug PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    MACOSX_BUNDLE_BUNDLE_NAME "Metal Slug"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.mslug.recomp"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PORT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PORT_VERSION}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks")

get_target_property(SDL3_BINARY SDL3::SDL3 IMPORTED_LOCATION)
if(NOT SDL3_BINARY)
    get_target_property(SDL3_BINARY SDL3::SDL3-shared IMPORTED_LOCATION)
endif()
if(NOT SDL3_BINARY)
    message(FATAL_ERROR "The pinned SDL3 framework location could not be determined")
endif()
get_filename_component(SDL3_FRAMEWORK_DIR "${SDL3_BINARY}" DIRECTORY)
add_custom_command(TARGET mslug POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:mslug>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_BUNDLE_CONTENT_DIR:mslug>/Frameworks/SDL3.framework"
    COMMAND /bin/cp -R
            "${SDL3_FRAMEWORK_DIR}" "$<TARGET_BUNDLE_CONTENT_DIR:mslug>/Frameworks/SDL3.framework"
    COMMENT "Embedding SDL3.framework")

install(TARGETS mslug BUNDLE DESTINATION .)

add_executable(mac_capabilities_test
    ${PROJECT_SOURCE_DIR}/tests/mac_capabilities.cpp port/mac_capabilities.mm)
target_include_directories(mac_capabilities_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/port)
target_link_libraries(mac_capabilities_test PRIVATE ${METAL_FRAMEWORK} ${FOUNDATION_FRAMEWORK})
set_target_properties(mac_capabilities_test PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
set_source_files_properties(port/mac_capabilities.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
add_test(NAME mac_capabilities COMMAND mac_capabilities_test)

if(EXISTS "${PD_GAME}")
    add_test(NAME verified_rom_set
             COMMAND mslug --roms ${PD_GAME} --headless --frames 1 --no-audio)
    set_tests_properties(verified_rom_set PROPERTIES
        ENVIRONMENT "PD_ROMS_INFO=1"
        PASS_REGULAR_EXPRESSION "MAME set, arcade program, BIOS us")
endif()

# Runtime tests need the user's complete set, not only the normalized private CPU inputs.
if(NOT EXISTS "${PD_GAME}")
    message(STATUS "Port tests disabled: the user's copy (PD_GAME) is missing")
    return()
endif()

# The loose MAME set and zips made in the build directory must load identical data.
if(EXISTS "${PD_GAME}/mslug" AND EXISTS "${PD_GAME}/neogeo")
    add_test(NAME roms_mame_folder COMMAND mslug --roms ${PD_GAME} --headless --frames 1 --no-audio)
    set_tests_properties(roms_mame_folder PROPERTIES ENVIRONMENT "PD_COMPARE_ROMS=${PD_GAME}")
    set(ZIPS ${CMAKE_CURRENT_BINARY_DIR}/mame_zips)
    add_test(NAME roms_mame_zips_make
             COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tests/loader/zip_set.py ${PD_GAME} ${ZIPS})
    set_tests_properties(roms_mame_zips_make PROPERTIES FIXTURES_SETUP mame_zips)
    add_test(NAME roms_mame_zip COMMAND mslug --roms ${ZIPS}/mslug.zip --headless --frames 1 --no-audio)
    set_tests_properties(roms_mame_zip PROPERTIES ENVIRONMENT "PD_COMPARE_ROMS=${PD_GAME}"
                         FIXTURES_REQUIRED mame_zips)
endif()

# Phase 5: a save state, loaded into a reset machine, reproduces RAM, image and audio.
add_test(NAME savestate_roundtrip
         COMMAND mslug --roms ${PD_GAME} --headless --frames 3000 --test-state 600)
