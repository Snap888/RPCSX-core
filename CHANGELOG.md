# Changelog — rpcsx core (Ouroboros fork)

All changes are on top of upstream `RPCSX/rpcsx` (dev). Every port lists the upstream
RPCS3 commit it derives from and the original author. ARM-specific changes are guarded
by `ARCH_ARM64`, so x86 builds are unaffected. Developed with AI assistance (Claude).

## v1.2.0 — build `v20260605-c9049b3`

### Open-world RSX crash hardening (inFamous long-session force-close)
Long open-world sessions on Adreno/Turnip could force-close with an RSX-thread
segfault. Root cause: under "Fast" RSX FIFO accuracy on weak-memory ARM, the
command processor can race ahead of the game and start executing stale /
uncommitted commands, whose corrupt offsets then drive an out-of-bounds memory
access. Both the navigation and the memory-access points are now guarded so a
desync **recovers** instead of crashing:
- **Transfer-engine destination bounds** — `NV0039` (`buffer_notify`) and
  `NV3089` (`image_in`) validated only the *start* of the destination, then
  copied the full strided extent. They now pass the real transfer length to
  `get_address` (which returns 0 on out-of-range) and skip / `recover_fifo`
  instead of `memcpy`-ing into unmapped host memory. (`NV308A` already did this.)
- **FIFO jump/CALL target validation** — `run_FIFO` followed jump/call targets
  unconditionally; a target in unmapped IO now triggers `recover_fifo` rather
  than executing whatever stale memory the GET pointer lands on.

> These convert the crash into the engine's existing recovery path. The
> underlying FIFO desync can be further reduced by raising **RSX FIFO Accuracy**
> above "Fast" (Advanced settings).

### Android memory / GPU
- **Device-adaptive VRAM budget, now user-respecting** — on ARM the Vulkan
  "device local" heap is shared system RAM, so the desktop default limit
  (65536 MB) let the caches grow into all of it (OOM / corruption in long
  sessions). When left at the default it now picks 2/3 of detected memory to
  leave headroom; **any explicit user value is honored as-is**. Scales to any
  device; x86 (dedicated VRAM) unaffected. *(refines the v1.1.0 clamp)*
- **Adreno classified as a driver vendor** — Qualcomm proprietary and Mesa
  Turnip were falling through to `unknown` (upstream RPCS3 doesn't classify them
  either). Detected by name and `VkDriverId`; groundwork for future Turnip-
  specific handling, no behaviour change yet.

> These are our own changes (developed with AI assistance), not ports from
> upstream RPCS3.

## v1.1.0 — build `v20260605-cdd53aa`

### Major — SPU recompiler re-vendored to upstream RPCS3
- **SPU LLVM Reduced Loop** — upstream's loop-detection pass; tighter, reduced-iteration codegen for hot SPU spin/wait loops. *(upstream a863e94c2 chain, Malcolm/Whatcookie)*
- **Authoritative ARM64 NEON recompiler** — `smull`/`umull`/`udot` and the proper TBL2/TBX2 register-scavenger **retry** (compile with TBL2, fall back per-program only if LLVM's allocator fails) replace the earlier blanket TBL1 fallback. *(a87d17529, Malcolm)*
- **Error-tolerant JIT** (`try_add`/`try_fin` + LLVM crash recovery) backs the retry. The Android decrementer still reads `CNTVCT_EL0`, not `readcyclecounter` (which lowers to `PMCCNTR_EL0` and traps on Android).

### Performance
- **PPU IR optimization enabled on ARM** — the PPU recompiler's `EarlyCSE`/analysis passes were gated behind `#ifdef ARCH_X64`; now run on ARM too, matching upstream and the SPU path (better codegen on the main game-code path).
- **JIT feature pinning** — advertise `+dotprod` so `UDOT` can be selected (fixes the ~20-minute "Cannot select AArch64ISD::UDOT" crash) and pin `+/-sha3/sve/sve2` to runtime HWCAP. *(completes b2469039a, Malcolm)*

### Correctness & stability
- **SPU `reservation_check` fast path fixed** — the always-allocated (vm::main/stack) path returned `== hash` while the slow path returns `!= hash`; a *changed* reservation was reported as still valid, so the lv2 waiter was never cleared (missed invalidation / stale reservation). Now consistent. *(65cd4deb7 portion, Elad)*
- **`named_thread_group` constructor** — wrong index/duplicate name for the final thread in the check-and-prepare path. *(d8710c431, Elad)*
- **cpu_thread abnormal-termination UAF** — the "terminated abnormally" warning was logged from the dying thread's torn-down TLS; now logged from a fresh thread. *(a1a140db9, Elad)*
- **`tar_object::save_directory` slicing** — `fs::dir_entry`→`fs::stat_t` slicing dropped saved-directory entry names (savestate/firmware). *(8c82ce8be, Megamouse)*
- **sys_fs dev_flash device-alias** — the `CELL_FS_IOS:` alias check looked for `BUILTIN_FLASH` on dev_flash2, but mounts are `BUILTIN_FLSH1/2/3`, so flash alias lookups never matched. *(9e0824112)*
- **Android VRAM headroom** — the Vulkan cache budget (`device_local_total_bytes`) could grow into the whole shared-memory "device local" heap (system RAM), starving the emulator/OS and causing OOM / unmapped-memory access-violation crashes in long open-world sessions (e.g. inFamous after a few minutes). Now clamped to 2/3 of detected memory on ARM unless the user sets a stricter VRAM limit. x86 (dedicated VRAM) is unaffected. *(ours)*

### Build
- **Embedded version no longer goes stale** — `v<date>-<hash>` is regenerated when HEAD moves instead of being frozen at first cmake configure.

> **Experimental / unverified:** this build compiles cleanly but the SPU re-vendor and the above need broad on-device game testing. SPU/PPU miscompiles show as in-game glitches, not build errors.

## v1.0.0

### ARM64 SPU/PPU recompiler performance
- **SMULL/UMULL for SPU integer multiplies** — lowers MPY/MPYS/MPYU/MPYI/MPYUI/MPYA to NEON widening multiplies (saves 1–2 instructions each). *(upstream 7e436f9bf, Malcolm)*
- **FCGT via BSL inline asm** — works around poor LLVM codegen for the SPU float-compare select on AArch64. *(320e8d634, Malcolm/Whatcookie)*
- **UDOT for SUMB** on dot-product–capable CPUs, plus a minimal `utils::has_dotprod()` (HWCAP_ASIMDDP) and `m_use_dotprod`. *(b2469039a + 4542020c8, Malcolm)*
- **NEON table lookups for SHUFB/VPERM** — SPU SHUFB and PPU VPERM use TBL/TBL2/TBX/TBX2 with constant-index folding instead of emulating x86 pshufb (SHUFB ~9 → ~5 instructions). *(dff29a786, Malcolm)*
- **Avoid the TBL2/TBX2 register-scavenger crash** — emit two TBL1/TBX1 lookups instead of TBL2/TBX2 (the same fallback upstream's retry path uses), eliminating a rare LLVM compile crash without the fragile JIT crash-recovery machinery. *(derived from a87d17529, Malcolm)*
- **Inline SPU decrementer via `CNTVCT_EL0`** — reads the decrementer timestamp inline using the architectural virtual counter (frequency = `CNTFRQ_EL0` = `get_tsc_freq()`), avoiding a host call. ARM-safe and more correct than upstream's `readcyclecounter` (which lowers to `PMCCNTR_EL0` and traps on most Android kernels). *(adapted from 61a260482, Malcolm)*
- **Faster reservation check** when the checked address shares a 1 MB/64 KB page with the current MFC effective address (skip the range-lock dance); also early-out if `SPU_EVENT_LR` is already pending. Arch-neutral. *(e6dc0d98f, Elad)*

### Correctness & stability
- **PPU analyser: fix possible infinite loop** from a `u32` overflow in the section probe. *(b08e80502, Elad)*
- **PPU reservation compare size `% 127` → `% 128`** — used the wrong cache-line granularity on cross-line reads. *(b41b10a03, Arsh Kumar Singh)*
- **RawSPU: bound ELF loads to the 256 KB local store** — a malformed ELF could memcpy past the buffer into host memory. *(b41b10a03, Arsh Kumar Singh)*
- **sys_spu: clamp image segments to local-store bounds** before copy/fill. *(7dce197ec, Elad)*
- **PPU analyser: use the 64-bit shift for SLDI** (was reading `op.sh32`). *(43b295892, RipleyTom)*
- **`ppu_register_function_at`: align range** so unaligned writes (e.g. via `sys_dbg_write_process_memory`) can't corrupt the interpreter cache. *(9deb6cd4f, FeTetra/Elad)*
- **Enable PPU vector-NaN fixup by default** (fewer graphical/physics glitches). *(8121bd443, Ani)*
- **cpu: prefetch each entry** in the suspend prefetch list (was always index 0). *(4ca0f0b11, Megamouse)*
- **cfg: avoid OOB read** in `try_to_enum_value`'s hex-prefix check. *(2b9368abf, Megamouse)*
- **vk: zero-init `queue_submit_t`** semaphore/stage arrays; bounds-checked inserts. *(5fc745086, Megamouse)*
- **trophies: return invalid id** (not `false`) when the trophy file is unreadable. *(9c719a585, Megamouse)*
- **rsx: fix swapped width/height** in the `NV309E_SET_FORMAT` swizzled-blit decoder. *(021f16f77, Phil Coulson)*
- **rsx: decode BC2/BC3 from unaligned source safely** — the software DXT path (used when the GPU lacks native BC, i.e. most Android devices) read blocks through a possibly-unaligned `u128` that can fault on ARM. *(09554c43b BC2/BC3 portion, kd-11)*

### Build fixes (this fork builds for arm64; the upstream dev branch did not)
- **Restore `#include "np_handler.h"`** in `signaling_handler.cpp` (dropped by the lib-split refactor; `np::np_handler`/context functions were undeclared).
- **Drop `constexpr`** on the four `StaticString` `vformat`/`assignVFormat` functions (`std::format_args` is not a literal type under NDK 29 clang).

### Misc
- Version string set to `1.0.0` (fork branch `ouroboros-arm64`).

### Not included (and why)
- **SPU "Reduced Loop"** optimization (the ~5–7% Cell improvement, RPCS3 ~Apr 2026): ~20 interdependent commits / ~2500 lines deep in the SPU analyser; will not graft onto this vendored tree by hand without high regression risk. Should arrive via a future upstream re-vendor.
- **Atomic cache-line alignment (64→128)**: bundled with semantic changes and edits to `lv2/` files that are restructured here.
