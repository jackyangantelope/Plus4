# Source provenance

| Directory | Source | License and local status |
| --- | --- | --- |
| `deps/plus4emu` | [istvan-v/plus4emu](https://github.com/istvan-v/plus4emu), `939e65233b227a47e5523eca149de8247176e28f` | GPL-2.0-or-later per source headers. `COPYING` retained. Source-only modified snapshot. |
| `deps/pico_shared` | [PicoPlus-devel/pico_shared](https://github.com/PicoPlus-devel/pico_shared/commit/df683bf00ff95e1ceda88f132bbf72f8cc7cadb0), revision `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` | GPL-3.0. `LICENSE` retained. Source snapshot. |
| `platform/wave1` | Fruit Jam integration adapted from the public [GameMaster release commit](https://github.com/jackyangantelope/GameMaster/commit/628b93f3c41e71130c6bb658a976460fca388c1a) | Adapted platform glue; repository GPLv3 license. |
| `platform/hdmi` | HDMI/HSTX driver derived from `pico_shared`, adapted for the port's audio transport | Modified derivative; repository GPLv3 license and upstream `deps/pico_shared/LICENSE` apply. |
| `firmware` | Plus/4-specific embedded glue and renderer/audio integration | Repository GPLv3 license; embedded Plus4Emu remains subject to its source headers. |

The Plus4Emu modifications are intentionally visible in `deps/plus4emu/src`. In particular, `cpu*` and `memory.cpp` adapt access for the embedded 64 KiB model; `ted*` and `render*` adapt video, sound, and manual frame stepping. `render032.inc` and `video_span038.hpp` are local additions used by those sources. This disclosure avoids presenting a modified vendor tree as an unaltered import.

No Commodore ROMs, game files, extracted game assets, or saves are included.
