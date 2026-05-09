# Cell Phone Demo

A phone UI demo for LVGL.
Pure C99, no external dependencies beyond LVGL and SDL2 (for the PC simulator).

![Home Screen](screenshots/02_home_screen.png)

## Quick start (macOS / Linux)

The fastest path is the bundled driver script `demos/cellphone/build.sh`.
It runs cmake/ninja for you, links the SDL host or test binary against
`liblvgl_demos.a`, and adapts linker flags per platform (no `-framework
Cocoa` on Linux).

```bash
demos/cellphone/build.sh           # build lvgl libs + cellphone_demo binary
demos/cellphone/build.sh demo      # build + launch the SDL demo window
demos/cellphone/build.sh test      # build + run the regression test
demos/cellphone/build.sh report    # test build with the heap-breakdown diagnostic
demos/cellphone/build.sh clean     # remove build/
demos/cellphone/build.sh help      # print usage
```

Outputs land in `build/`:
- `build/cellphone_demo` -- interactive SDL window
- `build/cellphone_test` -- non-interactive harness (`test` mode)
- `build/cellphone_test_report` -- same harness plus heap census (`report` mode)

Prerequisites: `cmake`, `ninja`, a C compiler, `sdl2` discoverable via
`pkg-config`. The script uses the cellphone config under
`demos/cellphone/config/` automatically.

### What the script runs (for reference)

If you prefer to drive the build by hand, the script collapses to:

```bash
# 1. Configure and build LVGL
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug \
  -DLV_BUILD_SET_CONFIG_OPTS=ON \
  -DLV_BUILD_CONF_DIR=demos/cellphone/config
cmake --build build --parallel

# 2. Build the demo host (drop `-framework Cocoa` on Linux)
cc -o build/cellphone_demo demos/cellphone/main_sdl.c \
   -I. -Ibuild -Idemos/cellphone/config -DLV_CONF_INCLUDE_SIMPLE \
   -Lbuild/lib -llvgl_demos -llvgl_examples -llvgl \
   $(pkg-config --cflags --libs sdl2) \
   -framework Cocoa -lpthread

# 3. Run
build/cellphone_demo
```

## Screen layout

Portrait QVGA (240 x 320):

```
+----------------------------+
| Status Bar (20 px)  HH:MM  |
+----------------------------+
|                            |
|      Content Area          |
|      240 x 268             |
|                            |
+----------------------------+
| Nav Bar (32 px)            |
|      [Back]   [Home]       |
+----------------------------+
```

The lock screen is full-screen and hides the status/nav bar until
the user slides to unlock.

## Applications

| App | Source file | Widget | Description |
|-----|-----------|--------|-------------|
| Lock screen | `lv_demo_cellphone_lock.c` | slider | Clock, date, slide-to-unlock |
| Home | `lv_demo_cellphone_home.c` | tileview + grid | 3x3 icon grid with page indicator |
| Phone | `lv_demo_cellphone_dialer.c` | buttonmatrix | 3x4 keypad, mock in-call screen |
| Contacts | `lv_demo_cellphone_contacts.c` | list | A-Z grouped list, detail view |
| Messages | `lv_demo_cellphone_sms.c` | list + labels | Conversation list, chat bubbles |
| Calculator | `lv_demo_cellphone_calc.c` | buttonmatrix | Working arithmetic (scaled integer) |
| Music | `lv_demo_cellphone_music.c` | list + slider | Track list, simulated playback |
| Photos | `lv_demo_cellphone_photo.c` | grid + tileview | 3x2 grid, fullscreen swipe |
| Settings | `lv_demo_cellphone_settings.c` | menu | Display, Sound, Wallpaper, About |
| Call Log | `lv_demo_cellphone_calllog.c` | custom list | Color-coded direction icons |

## Architecture

```
lv_demo_cellphone.c        Entry point, screen stack, app registry, clock helpers
lv_demo_cellphone_common.h Geometry, colors, fonts, shared types
lv_demo_cellphone_statusbar.c/h  Persistent top bar (real-time clock)
lv_demo_cellphone_navbar.c/h     Persistent bottom bar (Back / Home)
lv_demo_cellphone_data.c/h       Static datasets (contacts, SMS, call log, tracks)
lv_demo_cellphone_anim.c/h       Animation helpers (wobble, pickup, sweep)
```

### Screen stack

Navigation uses a push/pop stack (max depth 8). Each app screen is
created inside a content container positioned between the status bar
and nav bar. Transitions animate the X position of incoming and
outgoing pages.

```c
cellphone_screen_push(app_create_fn);   /* slide in from right */
cellphone_screen_pop();                 /* slide out to right   */
cellphone_screen_home();                /* fade all, back to home */
```

### Data layer

All demo data (12 contacts, 4 SMS threads, 15 call log entries,
10 music tracks) is compiled in as `static const` arrays in
`lv_demo_cellphone_data.c`. No filesystem, no SQLite.

## Assets

Source images live in `assets/png/`. Generated LVGL C arrays live in `assets/generated/`
(scaled to 40x40 RGB565A8). RGB565A8 stores RGB in two bytes plus an
8-bit alpha plane, matching `LV_COLOR_DEPTH 16` displays exactly while
saving 1.6 KiB per icon (12.5 KiB total) over ARGB8888.

To regenerate after modifying a source PNG (pre-scale to 40x40 first):

```bash
# Scale the source PNG (ImageMagick, Pillow, or any tool)
convert demos/cellphone/assets/png/phone.png -resize 40x40 /tmp/phone_40.png

# Convert to LVGL C array using the built-in converter
python3 scripts/LVGLImage.py --ofmt C --cf RGB565A8 \
    --name img_icon_phone \
    -o demos/cellphone/assets/generated \
    /tmp/phone_40.png
```

## Configuration

Enable in `lv_conf.h`:

```c
#define LV_USE_DEMO_CELLPHONE  1
```

Two opt-in configs ship with the demo:

| File | Profile | Notes |
|---|---|---|
| `demos/cellphone/config/lv_conf.h`     | SDL simulator | 1 MiB pool, logger on, snapshot/skin enabled |
| `demos/cellphone/config/lv_conf_mcu.h` | MCU shipping  | 96 KiB pool, no SDL/snapshot/skin/logger     |

Use either with `cmake -DLV_BUILD_CONF_DIR=demos/cellphone/config` (picks
`lv_conf.h`) or with the explicit `-DLV_BUILD_CONF_PATH=...lv_conf_mcu.h`.

Required LVGL features (both profiles): `LV_USE_FLEX`, `LV_USE_GRID`,
`LV_USE_LIST`, `LV_USE_MENU`, `LV_USE_TILEVIEW`, `LV_USE_BUTTONMATRIX`,
`LV_USE_SLIDER`, `LV_USE_ROLLER`, `LV_USE_FONT_VEC`. Bitmap Montserrat
fonts are not used; the vector font engine renders all text and FA5
icons. Per-font cache: per-instance L2 bitmap caches via
`lv_font_vec_init_ex()` (sm 2 KiB, normal 2 KiB; heading, large, and
clock disabled because their owning widgets repaint rarely enough that
on-demand rasterization is cheaper than holding the bitmaps) + a single
shared 16-set L1 metrics cache (~2.2 KiB, refcounted across all live
vec font instances). Configured L2 budget cap is 4 KiB; the true heap
footprint is higher (cache framework adds RB-tree nodes, entry headers,
and per-bitmap allocator overhead on top of the data payload), and is
best read from `lv_mem_monitor_t` after Test 11.

The MCU profile drops `LV_USE_SNAPSHOT` and `LV_USE_SDL` since neither
is reachable from the target host.

### MCU shipping profile

`lv_conf_mcu.h` sizes the LVGL pool only; the surrounding SRAM budget
(display draw buffer, libc heap, task stacks, app globals) is the
integrator's responsibility. Measurements come from `cellphone_test`
Test 11 reading the allocator high-water mark
(`lv_mem_monitor_t.max_used`), which captures every alloc/realloc
including intra-frame transients:

| Metric              | Value     | Note                                      |
|---------------------|-----------|-------------------------------------------|
| End-of-cycle pool   | 43 KiB    | Stable across 5 cycles (macOS)            |
| Peak pool           | 73 KiB    | Allocator high-water mark across full run |
| `LV_MEM_SIZE`       | 76 KiB    | Peak headroom 3 KiB (~4%)                 |

Four cache changes drove the current numbers. The shared L1 metrics
cache (one table refcounted across all vec font instances) recovered
14 KiB versus per-instance. The L1 set-count downsize from 32 to 16
sets recovered another 2.2 KiB while keeping the L1 hit rate at ~79%
(well above the 70% revert threshold; 8 sets dropped below it).
Per-font L2 budgets via `lv_font_vec_init_ex()` plus cache disable
for the heading (16 px), large (22 px), and clock (32 px) fonts
recovered ~11 KiB more -- those widgets repaint rarely enough that
on-demand rasterization is cheaper than holding bitmaps in the pool.
Halving the surviving sm/normal caps from 4 KiB to 2 KiB recovered
the last ~5 KiB; the live cache sits right at the cap and LRU
eviction is active. The 2 KiB choice is calibrated against new L2
hit/miss counters added in `src/font/lv_font_vec.c` and exposed via
`lv_font_vec_get_l2_stats(hits, misses)`: at 2 KiB the L2 hit rate
lands at ~70-72%, at 1.5 KiB it falls to 57%, and at 1 KiB it falls
to 43%. Test 11 gates on >= 60% to catch any future regression that
silently doubles the per-glyph rasterization rate; the heap savings
from going lower (-2 / -4 KiB at 1.5 / 1 KiB caps) aren't worth the
CPU cost on a slow Cortex-M0+ where a TTF rasterize is ~200-500 us
per 12-14 px glyph.

Indicative SRAM split for a 128 KiB-SRAM Cortex-M0+/M4-class target:

| Segment                                | Size      |
|----------------------------------------|-----------|
| LVGL pool (`LV_MEM_SIZE`)              | 76 KiB    |
| Two 30-line RGB565 partial draw buffers| ~28 KiB   |
| Task stacks + libc + app globals       | ~20 KiB   |
| **Total**                              | ~128 KiB  |

Notable departures from the simulator config:

- `LV_USE_SDL`, `LV_DEMO_CELLPHONE_SKIN`, `LV_USE_SNAPSHOT`, `LV_USE_LOG`
  all off (target has no host-side window or stdio).
- `LV_GRADIENT_MAX_STOPS` reduced from 6 to 2 (multi-stop gradients are
  unused by the demo).
- `LV_COLOR_DEPTH 16` and the RGB565A8 icons stay matched, so display
  blits land on exactly the right pixel format with no conversion.

The 128 KiB-SRAM tier is now the conservative default: 76 KiB pool +
two 30-line RGB565 partial draw buffers (~28 KiB) + stacks/libc/globals
(~20 KiB) = 128 KiB. A 112 KiB tier is reachable by switching to a
single 15-line draw buffer (saves ~14 KiB), totalling ~107 KiB; the
trade-off is more flush callbacks per frame and the slow renderer
path on partial scroll regions, so profile both options on the target
panel before committing. Going below 76 KiB pool requires upstream
LVGL contributions (label `ext_draw_size` without `spec_attr`,
size-segregated allocator path for transients).

## Automated test

`main_test.c` validates the full user journey (boot, unlock, launch
each app, back navigation) by structural gates only -- screen-stack
depth, object-tree counts, pool growth across five push/pop cycles,
per-font L2 cache budgets, and the L2 hit-rate band. Run it via:

```bash
demos/cellphone/build.sh test
```

113 checks cover boot, the lock-screen slide-to-unlock gesture (under-
threshold reset and full-slide pop), each app, the dialer call flow
(keypad accumulation, in-call screen push, 1 s timer flip, end-call
pop), the calculator arithmetic path (digits, +/=, divide-by-zero,
clear), per-cycle pool growth (with SMS chat-detail folded into each
cycle), per-font cache sizes, and the L2 hit-rate band (60% floor,
95% ceiling -- the ceiling catches workload-collapse regressions
where the cache "wins" only because the working set shrank).  The
harness writes nothing to disk; visual inspection is the SDL host's
job.

### Heap-breakdown diagnostic

`report` mode adds `mem_report.c` to the test link with
`-DCELLPHONE_TEST_REPORT`, which wires up the `cellphone_mem_report()`
call inside `main_test.c` after Test 11. It walks the live TLSF pool and
emits a per-class object/style census plus a block-size histogram --
useful for attributing pool occupancy when tuning the font caches or
chasing leak suspects.

```bash
demos/cellphone/build.sh report
```

Without the flag, `mem_report.c` collapses to an empty translation unit,
so `liblvgl_demos.a` carries no `printf`/`qsort`/private-header baggage.
The diagnostic itself requires `LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN`
(it walks the builtin TLSF pool); the file enforces this via `#error` when
the gate is active.

#### Calling `cellphone_mem_report()` from a custom host

The entry point is a public symbol, declared in
`demos/cellphone/mem_report.h`. To use it from your own integration:

```c
#include "demos/cellphone/mem_report.h"

run_my_workload();      /* push the system into a steady state first */
cellphone_mem_report();
```

Compile `demos/cellphone/mem_report.c` into the same target with
`-DCELLPHONE_TEST_REPORT`. Without the gate the function isn't defined
and the link fails -- intentional, so a missing build flag surfaces at
link time instead of producing a silently-empty report.

## License

Same as LVGL (MIT).
