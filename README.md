# Plus4

Commodore Plus/4 emulator source port for the public Adafruit Fruit Jam (RP2350, `HW_CONFIG=8`). This repository contains source code only. It does **not** include Commodore ROMs, games, or a standalone firmware image.

## Release status

The 0.4.14 manual-RAM emulator baseline passed basic user testing on RP2350B at 480 MHz: sound worked and the observed Kye scene held 50 frames/s over the measured interval. This Fruit Jam retarget compiles, but **has not been tested on Fruit Jam hardware**. Do not infer the same frame rate or audio reliability at its 252 MHz clock. Other games, prolonged play, and full compatibility remain unverified.

The firmware currently selects a `.prg` file and uses the Plus/4 BASIC and KERNAL ROMs supplied by the user. It is not a general-purpose disk or tape emulator, and keyboard coverage is limited by the available controller mapping.

## Build

Use a Pico SDK 2.3.x checkout with RP2350/Fruit Jam support, an ARM embedded toolchain, CMake, Ninja, [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB), and TinyUSB (normally the SDK's `lib/tinyusb`). On Windows, run:

```powershell
.\scripts\build-release.ps1 `
  -PicoSdkPath D:\path\to\pico-sdk `
  -PicoPioUsbPath D:\path\to\Pico-PIO-USB `
  -ToolchainPath C:\path\to\arm-toolchain `
  -NinjaPath C:\path\to\ninja.exe
```

If your SDK does not build `pioasm` and `picotool` automatically, also provide `-PioasmDir` and `-PicotoolDir`; `-TinyUsbPath` can override the SDK TinyUSB path. The output is `build/fruit-jam-release/picoPlus4.uf2`. It is linked at `0x10080000` for a compatible loader, **not** a bootloader or a standalone image for direct UF2 flashing. Do not flash it without confirming your loader's partition layout.

On the SD card, provide your own legally obtained 16 KiB `basic.rom` and 16 KiB `kernal.rom` at `/BIOS/PLUS4/`, and `.prg` files at `/roms/PLUS4/`. These files are intentionally ignored by Git and are not redistributed here.

## Source and licensing

The emulator is based on [istvan-v/plus4emu](https://github.com/istvan-v/plus4emu) revision `939e65233b227a47e5523eca149de8247176e28f`. `deps/plus4emu/src` is a **modified** source snapshot, not a clean upstream checkout. The upstream source headers permit GPL version 2 or later; its original [COPYING](deps/plus4emu/COPYING) is preserved. The embedded changes touch `cpu.cpp`, `cpu.hpp`, `memory.cpp`, `plus4emu.hpp`, `render.cpp`, `ted.hpp`, `ted_init.cpp`, `ted_main.cpp`, and `ted_snd.cpp`, and add `render032.inc` and `video_span038.hpp`. They adapt memory and CPU access, TED rendering and sound, and the embedded/manual execution path. See [THIRD_PARTY.md](THIRD_PARTY.md) for the component map.

The public Fruit Jam runtime is adapted from the [GameMaster port](https://github.com/jackyangantelope/GameMaster/commit/628b93f3c41e71130c6bb658a976460fca388c1a). `deps/pico_shared` comes from [PicoPlus-devel/pico_shared](https://github.com/PicoPlus-devel/pico_shared) revision `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` and carries its own [GPLv3 license](deps/pico_shared/LICENSE). The `platform/hdmi` copy is modified for HDMI audio, not a pristine vendor snapshot. The repository-level [LICENSE](LICENSE) is GPLv3; third-party notices and source-file headers remain applicable to their respective files.

The build also consumes Pico SDK, Pico-PIO-USB, TinyUSB, and their dependencies from your local toolchain. Their licensing is controlled by those upstream projects; they are not copied into this repository.
