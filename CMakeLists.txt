cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "VitaSDK toolchain file")
  else()
    message(FATAL_ERROR "VITASDK not set and no toolchain file specified")
  endif()
endif()

project(goldbox-vita C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# ---------------------------------------------------------------------------
# Per-game settings (override with -DGAME=gateway etc.)
# ---------------------------------------------------------------------------
set(GAME "gateway" CACHE STRING "Which game to build")

if(GAME STREQUAL "gateway")
  set(VITA_APP_NAME "Gateway to the Savage Frontier")
  set(VITA_TITLEID  "GBOX00001")
elseif(GAME STREQUAL "treasures")
  set(VITA_APP_NAME "Treasures of the Savage Frontier")
  set(VITA_TITLEID  "GBOX00002")
else()
  message(FATAL_ERROR "Unknown GAME: ${GAME}")
endif()

set(VITA_VERSION "01.00")

# ---------------------------------------------------------------------------
# DOSBox source
# ---------------------------------------------------------------------------
set(DOSBOX_SRC "${CMAKE_SOURCE_DIR}/dosbox/src/src")

set(DOSBOX_SOURCES
  ${DOSBOX_SRC}/dosbox.cpp
  # cpu
  ${DOSBOX_SRC}/cpu/callback.cpp
  ${DOSBOX_SRC}/cpu/core_dynrec.cpp
  ${DOSBOX_SRC}/cpu/core_full.cpp
  ${DOSBOX_SRC}/cpu/core_normal.cpp
  ${DOSBOX_SRC}/cpu/core_prefetch.cpp
  ${DOSBOX_SRC}/cpu/core_simple.cpp
  ${DOSBOX_SRC}/cpu/cpu.cpp
  ${DOSBOX_SRC}/cpu/flags.cpp
  ${DOSBOX_SRC}/cpu/modrm.cpp
  ${DOSBOX_SRC}/cpu/paging.cpp
  # dos
  ${DOSBOX_SRC}/dos/cdrom_image.cpp
  ${DOSBOX_SRC}/dos/cdrom.cpp
  ${DOSBOX_SRC}/dos/dos_classes.cpp
  ${DOSBOX_SRC}/dos/dos_devices.cpp
  ${DOSBOX_SRC}/dos/dos_execute.cpp
  ${DOSBOX_SRC}/dos/dos_files.cpp
  ${DOSBOX_SRC}/dos/dos_ioctl.cpp
  ${DOSBOX_SRC}/dos/dos_keyboard_layout.cpp
  ${DOSBOX_SRC}/dos/dos_memory.cpp
  ${DOSBOX_SRC}/dos/dos_misc.cpp
  ${DOSBOX_SRC}/dos/dos_mscdex.cpp
  ${DOSBOX_SRC}/dos/dos_programs.cpp
  ${DOSBOX_SRC}/dos/dos_tables.cpp
  ${DOSBOX_SRC}/dos/dos.cpp
  ${DOSBOX_SRC}/dos/drive_cache.cpp
  ${DOSBOX_SRC}/dos/drive_fat.cpp
  ${DOSBOX_SRC}/dos/drive_iso.cpp
  ${DOSBOX_SRC}/dos/drive_local.cpp
  ${DOSBOX_SRC}/dos/drive_virtual.cpp
  ${DOSBOX_SRC}/dos/drives.cpp
  # fpu
  ${DOSBOX_SRC}/fpu/fpu.cpp
  # gui
  ${DOSBOX_SRC}/gui/midi.cpp
  ${DOSBOX_SRC}/gui/render_scalers.cpp
  ${DOSBOX_SRC}/gui/render.cpp
  ${DOSBOX_SRC}/gui/sdl_gui.cpp
  ${DOSBOX_SRC}/gui/sdl_mapper.cpp
  ${DOSBOX_SRC}/gui/sdlmain.cpp
  # hardware
  ${DOSBOX_SRC}/hardware/adlib.cpp
  ${DOSBOX_SRC}/hardware/cmos.cpp
  ${DOSBOX_SRC}/hardware/dbopl.cpp
  ${DOSBOX_SRC}/hardware/disney.cpp
  ${DOSBOX_SRC}/hardware/dma.cpp
  ${DOSBOX_SRC}/hardware/gameblaster.cpp
  ${DOSBOX_SRC}/hardware/gus.cpp
  ${DOSBOX_SRC}/hardware/hardware.cpp
  ${DOSBOX_SRC}/hardware/iohandler.cpp
  ${DOSBOX_SRC}/hardware/joystick.cpp
  ${DOSBOX_SRC}/hardware/keyboard.cpp
  ${DOSBOX_SRC}/hardware/mame/saa1099.cpp
  ${DOSBOX_SRC}/hardware/mame/sn76496.cpp
  ${DOSBOX_SRC}/hardware/memory.cpp
  ${DOSBOX_SRC}/hardware/mixer.cpp
  ${DOSBOX_SRC}/hardware/mpu401.cpp
  ${DOSBOX_SRC}/hardware/opl.cpp
  ${DOSBOX_SRC}/hardware/pcspeaker.cpp
  ${DOSBOX_SRC}/hardware/pic.cpp
  ${DOSBOX_SRC}/hardware/sblaster.cpp
  ${DOSBOX_SRC}/hardware/serialport/misc_util.cpp
  ${DOSBOX_SRC}/hardware/serialport/serialport.cpp
  ${DOSBOX_SRC}/hardware/tandy_sound.cpp
  ${DOSBOX_SRC}/hardware/timer.cpp
  ${DOSBOX_SRC}/hardware/vga_attr.cpp
  ${DOSBOX_SRC}/hardware/vga_crtc.cpp
  ${DOSBOX_SRC}/hardware/vga_dac.cpp
  ${DOSBOX_SRC}/hardware/vga_draw.cpp
  ${DOSBOX_SRC}/hardware/vga_gfx.cpp
  ${DOSBOX_SRC}/hardware/vga_memory.cpp
  ${DOSBOX_SRC}/hardware/vga_misc.cpp
  ${DOSBOX_SRC}/hardware/vga_other.cpp
  ${DOSBOX_SRC}/hardware/vga_paradise.cpp
  ${DOSBOX_SRC}/hardware/vga_s3.cpp
  ${DOSBOX_SRC}/hardware/vga_seq.cpp
  ${DOSBOX_SRC}/hardware/vga_tseng.cpp
  ${DOSBOX_SRC}/hardware/vga_xga.cpp
  ${DOSBOX_SRC}/hardware/vga.cpp
  # ints
  ${DOSBOX_SRC}/ints/bios_disk.cpp
  ${DOSBOX_SRC}/ints/bios_keyboard.cpp
  ${DOSBOX_SRC}/ints/bios.cpp
  ${DOSBOX_SRC}/ints/ems.cpp
  ${DOSBOX_SRC}/ints/int10_char.cpp
  ${DOSBOX_SRC}/ints/int10_memory.cpp
  ${DOSBOX_SRC}/ints/int10_misc.cpp
  ${DOSBOX_SRC}/ints/int10_modes.cpp
  ${DOSBOX_SRC}/ints/int10_pal.cpp
  ${DOSBOX_SRC}/ints/int10_put_pixel.cpp
  ${DOSBOX_SRC}/ints/int10_vesa.cpp
  ${DOSBOX_SRC}/ints/int10_video_state.cpp
  ${DOSBOX_SRC}/ints/int10_vptable.cpp
  ${DOSBOX_SRC}/ints/int10.cpp
  ${DOSBOX_SRC}/ints/mouse.cpp
  ${DOSBOX_SRC}/ints/xms.cpp
  # libs
  ${DOSBOX_SRC}/libs/gui_tk/gui_tk.cpp
  # serialdummy is always needed — provides the null serial device
  ${DOSBOX_SRC}/hardware/serialport/serialdummy.cpp
  # zmbv (video capture codec) excluded: needs zlib and is only for C_SSHOT
  # misc
  ${DOSBOX_SRC}/misc/cross.cpp
  ${DOSBOX_SRC}/misc/messages.cpp
  ${DOSBOX_SRC}/misc/programs.cpp
  ${DOSBOX_SRC}/misc/setup.cpp
  ${DOSBOX_SRC}/misc/support.cpp
  # vita stubs for missing POSIX functions
  ${CMAKE_SOURCE_DIR}/dosbox/vita/vita_shims.c
  # shell
  ${DOSBOX_SRC}/shell/shell_batch.cpp
  ${DOSBOX_SRC}/shell/shell_cmds.cpp
  ${DOSBOX_SRC}/shell/shell_misc.cpp
  ${DOSBOX_SRC}/shell/shell.cpp
  # Gold Box overlay: NewLetterButtons, VitaButtons, IME
  ${CMAKE_SOURCE_DIR}/src/overlay/vita_overlay.cpp
  ${CMAKE_SOURCE_DIR}/src/overlay/vita_journal.cpp
)

# ---------------------------------------------------------------------------
# Build target
# ---------------------------------------------------------------------------
add_executable(dosbox-vita ${DOSBOX_SOURCES})

target_include_directories(dosbox-vita PRIVATE
  # Our vita config.h overrides the autoconf-generated one
  ${CMAKE_SOURCE_DIR}/dosbox/vita
  ${DOSBOX_SRC}/../include
  ${DOSBOX_SRC}
  $ENV{VITASDK}/arm-vita-eabi/include/SDL
  ${DOSBOX_SRC}/cpu
  ${DOSBOX_SRC}/dos
  ${DOSBOX_SRC}/fpu
  ${DOSBOX_SRC}/gui
  ${DOSBOX_SRC}/hardware
  ${DOSBOX_SRC}/hardware/mame
  ${DOSBOX_SRC}/hardware/serialport
  ${DOSBOX_SRC}/ints
  ${DOSBOX_SRC}/libs/gui_tk
  ${DOSBOX_SRC}/libs/zmbv
  ${DOSBOX_SRC}/misc
  ${DOSBOX_SRC}/shell
  ${CMAKE_SOURCE_DIR}/src/overlay
)

target_compile_definitions(dosbox-vita PRIVATE
  HAVE_CONFIG_H
  # Suppress MIDI device probe that doesn't work on Vita
  DISABLE_MIDI_DEVICE_ENUM
  GOLDBOX_GAME="${GAME}"
)

target_compile_options(dosbox-vita PRIVATE
  -O2
  -ffast-math
  # Suppress warnings from vendored DOSBox code
  -Wno-narrowing
  -Wno-write-strings
  -Wno-misleading-indentation
)

target_link_libraries(dosbox-vita
  SDL
  SDL_mixer
  # Vita system stubs (SDL1.2 pkg-config pulls these but we list them explicitly)
  SceGxm_stub
  SceDisplay_stub
  SceCtrl_stub
  SceAppMgr_stub
  SceAudio_stub
  SceIofilemgr_stub
  SceCommonDialog_stub
  SceTouch_stub
  SceHid_stub
  SceMotion_stub
  ScePower_stub
  SceProcessmgr_stub
  SceNet_stub
  SceNetCtl_stub
  m
)

# ---------------------------------------------------------------------------
# VPK packaging
# ---------------------------------------------------------------------------
vita_create_self(dosbox-vita.self dosbox-vita UNSAFE)

vita_create_vpk(${GAME}.vpk ${VITA_TITLEID} dosbox-vita.self
  VERSION ${VITA_VERSION}
  NAME    ${VITA_APP_NAME}
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/dosbox.conf dosbox.conf
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/overlay.json overlay.json
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/pad.json pad.json
  # LiveArea art: tools/livearea/make_art.py (original drawing, no box art)
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/livearea/icon0.png sce_sys/icon0.png
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/livearea/bg.png sce_sys/livearea/contents/bg.png
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/livearea/startup.png sce_sys/livearea/contents/startup.png
  FILE    ${CMAKE_SOURCE_DIR}/configs/${GAME}/livearea/template.xml sce_sys/livearea/contents/template.xml
)
