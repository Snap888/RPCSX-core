# Changelog — rpcsx core (Ouroboros fork)

All changes are on top of upstream `RPCSX/rpcsx` (dev). Every port lists the upstream
RPCS3 commit it derives from and the original author. ARM-specific changes are guarded
by `ARCH_ARM64`, so x86 builds are unaffected. Developed with AI assistance (Claude).

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
