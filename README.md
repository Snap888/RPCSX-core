# RPCSX-Clanker — core (experimental)

The **PlayStation 3** emulation core (RPCS3-derived) used by the
[RPCSX-Clanker Android app](https://github.com/Ouroboros420/rpcsx-ui-android),
built into `librpcsx-android.so` for arm64. A continuously-updated fork of
[RPCSX/rpcsx](https://github.com/RPCSX/rpcsx), synced to **RPCS3 0.0.41** content
for the emulation core with extensive ARM64/Android adaptation. It installs as the
**`net.rpcsx.clanker`** package so it runs side-by-side with official RPCSX.

---

## Two requests if you use this build

This fork is **not my work** — it is the work of the **RPCS3** and **RPCSX** teams.
I only ported and adapted what they built onto Android, with heavy AI assistance.
If you get any enjoyment out of it, please honour these two things:

### 1. Donate to the teams who actually built the emulator
Every line of real emulation here came from them. Support them, not me — I want nothing:
- **RPCS3** — <https://rpcs3.net/> · Patreon: <https://www.patreon.com/Nekotekina>
- **RPCSX** — <https://github.com/RPCSX/rpcsx>

### 2. Play Demon's Souls online — and find me
The shadows finally work. Boot **Demon's Souls**, go online via RPCN, and come find
me in the Nexus (leave a message / soapstone). That's the only reward I'm after.

---

### Honest note on how it's made
The work of merging/porting these RPCS3 changes into this restructured tree is done
with heavy **AI assistance** (Claude). Treat it as **experimental** — verify before
relying on it, and expect rough edges.

### Why this exists
Not to splinter off yet another permanent fork of the PS3 emulator. The goals are
simply to **keep this build current**, and to **help the real RPCSX / RPCS3 teams**
by demonstrating what's possible — they are welcome to take any idea or change from
here. All credit for the emulator belongs to the **RPCS3** and **RPCSX** developers.

> Piracy is not permitted. Do not ask for games or system files.

---

## Things to be cautious about (read before relying on it)

<details open>
<summary><b>Known issues, trade-offs &amp; gotchas</b></summary>

- **Experimental & AI-assisted.** This tree is restructured and the porting is done with
  AI help. Expect bugs, regressions between builds, and behaviour that differs from
  official RPCS3/RPCSX. Do not use it as a reference for "how RPCS3 behaves."
- **Fresh, separate install.** The package was renamed to `net.rpcsx.clanker` for
  side-by-side use with official RPCSX. That means a **clean, empty install** — your
  games list, configs, saves, caches and firmware are **not** carried over from another
  RPCSX/RPCSX install. Set up firmware and games again.
- **Cast-shadow fix trade-off.** The Demon's Souls shadow fix works by skipping the
  fragment alpha-test in depth-only (shadow-caster) passes. Side effect: **alpha-tested
  geometry (foliage/grass, chain-link, etc.) casts a solid rather than a cut-out shadow**
  in some games. Minor, and color rendering is unaffected, but it is a deliberate
  approximation.
- **RPCN online toggle is not fully authoritative yet.** A **per-game config** can pin
  the title online, which overrides the global RPCN enable/disable toggle at game boot,
  and an already-open session is not torn down on disable. To reliably go offline you may
  need to also clear the per-game config's net/online setting. (A proper fix is on the
  TODO.)
- **RPCN secrets are stored in cleartext** in `rpcn.yml` on app-accessible storage
  (derived password + token). Avoid RPCN on a shared/untrusted device until this is
  hardened (TODO). TLS certificate verification for RPCN is also off (same as upstream).
- **Battery-saver is default ON** (it cut idle CPU with no fps cost on the test device).
  If you ever suspect it, you can turn it off in the app. All *other* power features
  (big-cluster affinity, low-power WFE, thermal cap, ADPF hints) are **default OFF and
  unproven** — enabling them may regress fps or stability; treat them as experiments.
- **Smooth-shaders (async interpreter) is forced OFF** — that path currently freezes
  (black screen/ANR). The synchronous interpreter is used instead (occasional first-use
  stutter, but it never hangs).
- **No persistent SPU object cache.** The on-disk SPU cache was removed (it wrote nothing
  on-device), so SPU code is recompiled each launch — expect a short warm-up every boot.
- **First-launch compilation is heavy.** PPU/shader compilation on first boot of a game is
  RAM- and CPU-intensive; an OOM mitigation (device-scaled compile budget) is in place but
  low-RAM devices may still struggle.
- **Not all games work.** Some titles still crash or stall (e.g. God of War 3 / SPURS-stall
  class). This is an emulator-wide reality, not specific to this fork.
- **arm64 only**, targeting recent Adreno/Turnip (Snapdragon 8 Gen 2 class). Other GPUs/SoCs
  are untested.
- **LLVM version.** Builds are intended to use LLVM 19.1.7 (to match RPCS3) but the shipped
  builds actually run **LLVM 20.1.3** due to a build-config drift (see Tried/Reverted).
- **Savestates** may not be compatible across versions.
</details>

---

## Changes vs upstream RPCSX (baseline `b41e09a04`)

<details>
<summary><b>Android platform &amp; runtime</b></summary>

- **LLVM-target auto-detection** — detects the real SoC big/prime core via MIDR instead of a fixed CPU target; registers all LLVM targets; guards against in-order-core misdetection; pins LLVM features to runtime HWCAP (sha3/dotprod/sve).
- **Crash logging** — native backtrace on Android crashes; SIGABRT and SIGBUS captured by the crash logger.
- **Shutdown / lifecycle fixes** — fixed the Emulation Join Thread self-join deadlock (shutdown hang); implemented `qt_events_aware_op` (was an empty stub) to fix stop sequencing; don't `ensure()`-crash on surface loss when no pad thread exists.
- **RLIMIT_MEMLOCK** raised so hot guest pages (vm/main/stack) can be pinned against long-session reclaim (ported from aps3e, credited).
- **Compile-thread management** — lower compile-thread priority (`nice`) on Android to reduce ANRs during shader/PPU compile; Android compile-thread cap applied to the effective thread count.
- **Device-scaled PPU compile memory budget** — fixes OOM SIGABRT during first-launch concurrent PPU LLVM module compilation; budget derived from ActivityManager-reported usable memory (pushed via JNI), since `get_total_memory()` over-reports on Android (zRAM).
- **Log de-noising** — high-traffic channels raised for release builds; benign memory-lock failure logged once at warning level.
- **Package rename** to `net.rpcsx.clanker` with applicationId-derived DocumentsProvider authority (avoids `INSTALL_FAILED_CONFLICTING_PROVIDER` alongside official RPCSX).
</details>

<details>
<summary><b>CPU, recompilers &amp; JIT (PPU / SPU / LLVM / ARM64)</b></summary>

- **Full SPU recompiler re-vendor** to current upstream (the "Reduced Loop" engine) plus upstream's authoritative ARM intrinsic codegen.
- **ARM64 SPU codegen**: NEON table lookups for SHUFB/VPERM; SMULL/UMULL for integer multiplies; UDOT for SUMB on dotprod CPUs; ARMv8.6 I8MM/dotprod for GBH/GBB gather-bits; TBL for ROTQBY-family shuffles; idiomatic FSM/FSMH/FSMB/FSMBI lowering; BSL-for-FCGT via inline asm (LLVM codegen workaround).
- **ARM64 SPU stability**: stopped emitting fragile NEON tbl2/tbx2 (SPU register corruption); don't advertise SVE/SVE2 to LLVM (SPU miscompile); inline SPU decrementer via `CNTVCT_EL0` (Android-correct; `readcyclecounter` traps on most devices).
- **PPU**: enabled LLVM IR optimization (EarlyCSE) on ARM (was x64-gated off); disabled the branch-folding loop that miscompiles (Asura's Wrath, RPCS3 #18287); analyser infinite-loop fix; 64-bit shift for SLDI; vector-NaN fixup default on; reservation compare size 127→128; PPU reservation priority over SPUs.
- **SPU correctness**: `spu_channel` occupy/wait bit-collision fix (Uncharted 2 SPURS hang); mis-ported cache-line waiter fix (SPURS work-queue corruption); restored dropped GPR-barrier guard in store elimination; double-check reservation data before PUTLLC writeback; full reservation-notification reimplementation; restored `COOPERATE_WITH_SYSTEM` guards; implemented `sys_spu_image_open_by_fd` (was NULL).
- **JIT**: targeting LLVM 19.1.7 to match upstream RPCS3 (see Tried/Reverted — on-disk builds drifted to 20.1.3); versioned compiled-code caches (PPU `v9-kusa`) so codegen fixes reach existing installs; codegen targets the big/prime ARM core; ARM `busy_wait` scaled to the hardware timer; `isb` for pause.
</details>

<details>
<summary><b>Graphics — Vulkan / RSX</b></summary>

- **Vulkan barrier/hazard correctness ports** (upstream): WAW/RAW texture-cache flush hazards; scratch-buffer barriers around the CPU detiler and texture uploads; barrier before aggregating sections into a texture; occlusion-query copy barrier; LATE_FRAGMENT_TESTS paired with EARLY in depth-stencil barriers; cubemap detection with extra creation flags; texture-cache lifetime/hazard fixes; locked descriptor dispatch notification list.
- **Adreno / Turnip support**: classify Adreno (Mesa Turnip + Qualcomm proprietary) as a driver vendor; fixed an Adreno compute workgroup-size regression (1→128); recognize Adreno in remaining driver-vendor switches.
- **Mobile VRAM budget** — caps device-local cache to 2/3 of detected memory on shared-memory ARM (fixes inFamous-class OOM / unmapped-memory crashes from caches growing into all RAM); only auto-picks when the user left the limit at default.
- **FIFO / transfer-engine crash hardening** — bounds-check transfer destination extents; recover instead of following FIFO jumps/calls into unmapped IO; bounds-check vertex upload source and index arrays against stale-draw wild reads/writes; skip textures with an unmapped source offset (converts a class of RSX desync segfaults into FIFO recovery).
- **Texture / format fixes** — deswizzle of wide texel formats; safe BC2/BC3 decode from unaligned source; fixed swapped width/height in `NV309E_SET_FORMAT`; weak vertex-cache modified-data detection.
- **Android-specific (ahead of upstream)** — abort-safe fence/event guards (savestate-freeze fix upstream lacks); Android surface-lost swapchain recreate; ARM64 FIFO idle-wait/WFE park; async pipeline-compiler worker heuristic tuned above upstream's cap; Async Texture Streaming 2 boot-crash fix.
- **Savestate / surface lifecycle** — track and destroy all Android WSI surfaces; release on swapchain reinit to fix `VK_ERROR_NATIVE_WINDOW_IN_USE` on savestate reload.
</details>

<details>
<summary><b>Shaders</b></summary>

- **Fragment-program decompiler ported to upstream 0.0.41** — adopted upstream's Assembler engine (CFG/IR + RegisterAnnotation/RegisterDependency lane-mask passes) wholesale, replacing the fork's legacy decompiler. Closes the one default-render-path subsystem that was a generation behind.
- **0.0.41 ROP / alpha-test / depth-compare shader specialization** — compile-time per-shader ROP specialization instead of runtime branches; SPIR-V 1.5 path.
- **Producer-side fixes the fork lacked** — raise `TEXTURE_FORMAT_CONVERT` so texel conversion actually emits (fixed disappearing geometry; **enabled vegetation/foliage rendering**); produce/consume `DISABLE_EARLY_Z` for cyclic-zeta draws; gate fragment color outputs by `mrt_buffers_count`.
- **Backend bridges** — emit the fp16 GLSL extension whenever the GPU supports it; bridge fragment constants to upstream's indexed `_fetch_constant` model (VK and GL).
- **Demon's Souls cast-shadow fix** (Android, validated on-device) — suppress the alpha-test discard in depth-only / shadow-caster passes so the full character casts a shadow (was torso-only). Trade-off in the Cautions section.
- **ShaderInterpreter variant engine** ported (backend-agnostic).
</details>

<details>
<summary><b>Emulation core — lv2 / HLE / loader / crypto</b></summary>

- **Massive HLE (ps3fw) re-vendor to current RPCS3** — dozens of modules: cellSysutil, cellSaveData, cellGame, cellSysCache, cellFs, cellFont, cellHttp/HttpUtil, cellSsl, cellSync/Sync2, cellRtc, cellL10n, cellVideoOut, cellAudio/AudioOut, cellMsgDialog, cellOskDialog, cellUserInfo, sceNpTrophy/Tus/Sns/Commerce2/Matching, cellRec/Screenshot/Voice, sysPrxForUser, and many more.
- **FMV / media stack** — re-vendored the demuxer (`cellDmuxPamf`, SPU-accurate) for FMV freeze fixes; re-vendored cellVdec/cellAdec/cellAtracXdec; added missing vdec decoder-library modules (fixed `cellVdecQueryAttr` crash); added audit-found modules (libsvc1d, sceNpBasicLimited).
- **lv2 kernel fixes** — lazy shm allocation (savestate v2); release event port on free_address; `sys_cond` deferred-init raw-mutex pointer; `sys_fs` op_read fast-path + `check_addr` guard; widen `_sys_ppu_thread_create` stack size to u64; scan 4 SPU reservation-waiter slots in `notify_all`; flush postponed reservation notification at `sys_mutex_unlock`; SPU event-notification ordering vs queue/group mutexes; dev_flash device-alias resolution; `open_raw` error-code fidelity.
- **Loader / crypto** — transcription-slip fixes (memcpy/memcmp, rel-exec); align range in `ppu_register_function_at`; clamp ELF/SPU segments to local-store bounds; ISO fail-open on short extent read + clamped directory reads.
- **Savestate fidelity** — serialize PPU scalar registers **unconditionally** (version-gate desync fix); re-`Init()` on reload so firmware VFS mounts survive; fixed savestate-stop crash in `has_flushable_data`; abort-break RSX flush waits on stop.
- **Version** synced to 0.0.41.
</details>

<details>
<summary><b>Audio</b></summary>

- **cellAudio re-vendor** — `_mxr000` event-queue fixes; unblocked via `video_provider` const-correctness fixes.
- OOB-read guard in `cellMusicDecode::decode_read`.
- (Audio soft-clip uses the upstream `std::clamp`, not the "improved tanh" variant some reference trees carry.)
</details>

<details>
<summary><b>Networking / RPCN</b></summary>

- **Full Emu/NP re-vendor to protocol 30** — swapped FlatBuffers (`fb_helpers`) for **Protobuf** (`pb_helpers` + generated `np2_structs.pb`), wiring protobuf + abseil into the 3rdparty Android arm64 cross-compile. Ported clans SDK symbols (full clans HLE deferred).
- **Android RPCN JNI bridge** — `_rpcsx_rpcn*` for config/credentials, host list, account create / resend-token / test-connection, enable/disable.
- **Client-side password derivation** (PBKDF2-HMAC-SHA3-256), since the fork has no Qt layer that previously did it; fixed retry / host-switch state caching.
- **Privacy** — never persist RPCN secrets to `rpcn.yml`; redact other users' NPIDs/names from netplay logs. (See Cautions re: cleartext storage / TLS.)
</details>

<details>
<summary><b>Power &amp; thermal (Android)</b></summary>

- **Battery-saver core override** (default ON) — clamps the SPU GETLLAR busy-wait so idle reservation polls take the OS sleep instead of pinning a big core (the #1 steady-state SPU drain); on-device A/B measured a meaningful absolute CPU drop with negligible fps cost. Prefers FIFO/FIFO_RELAXED present under battery-saver.
- **Experimental big-cluster affinity** (default OFF) — detects the big cluster via cpufreq and biases PPU/SPU/RSX threads; opt-in (over-subscription can cost fps).
- **Experimental low-power WFE waiting** (default OFF) — `ldaxr+wfe` park wired into the RSX semaphore-acquire and FIFO-empty idle spins, with a hot pre-spin to avoid frametime jitter; a power-vs-smoothness dial.
- **Thermal-aware frame cap** + ADPF performance-hint feed (default OFF, advisory; self-disables on devices whose power-HAL returns no session).
</details>

<details>
<summary><b>App-facing core features &amp; build system</b></summary>

- JNI for per-game configs (store only edited settings, inherit the rest) + community-config import; patch engine (list/toggle/version); game version + titles in patch JSON; power/thermal/affinity/WFE toggles; RPCN.
- In-game home-menu re-vendor (reworked overlay home/quick menu, SDF rendering wired into the VK backend) + a "Clanker Features" live-toggle tab; quick-menu toggle crash fixed (overlay button-press racing the flip thread's vertex upload — now locks the display manager).
- Calendar/commit-time **CalVer** version naming.
- Build: `USE_LLVM_VERSION` overridable with `LLVM_PREBUILT_ROOT`; Protobuf + abseil wired into 3rdparty (`flatc` step dropped); per-commit version refresh.
</details>

## Tried but failed / reverted

<details>
<summary>Expand — the honest list of dead ends</summary>

- **Async shader interpreter ("smooth shaders")** — ported upstream's async interpreter; `preload()`'s block-drain parks the RSX thread (black screen / ANR, all cores 0%). Reverted twice; **default OFF**. A non-blocking Strategy-B re-land is specced but unshipped.
- **The Demon's Souls cast-shadow rabbit hole (~6 builds, all on-device tested, all reverted before the real fix)** — receiver-side DEPTH_FLOAT/format_class coercion; col0 lane-completion seed; D32_SFLOAT compat-list widening; alpha-test always-pass (early flawed test); fp32 z-clip full-range remap (added per-vertex cost / choppiness). The fp64-z-clip theory was disproven on-device. The eventual fix was suppressing the depth-only alpha-test discard.
- **`format_ex` / bx2 texel-conversion rework** — under-raised conversion vs the known-good broad `TEXTURE_FORMAT_CONVERT`, causing see-through torsos and missing vegetation; reverted to the known-good path.
- **Dirty-flag re-specialization** — byte-matched upstream 0.0.41 but unmasked a fork-specific alpha-test discard that made torsos/vegetation vanish; reverted. Lesson: "matches upstream" ≠ "safe on this fork."
- **Per-fragment alpha-conversion preservation + cache bump** — theorized raw-alpha fix; on-device probe proved it irrelevant; reverted (the cache bump also caused a recompile storm).
- **SPU native-object disk cache** — versioned/keyed disk cache for compiled SPU objects; wrote **zero** objects on-device across three investigations; removed (also an unbounded-storage concern), back to the in-memory SPU JIT.
- **EMULATE_DEPTH_COMPARE / MULTISAMPLED_ZBUFFER + cyclic-Z DISABLE_EARLY_Z producers** — landed, then deliberately reverted as unproven/orphaned during the shadow-saga cleanup.
- **`util` ASLR/fat_ptr atomic-wait + atomic_ptr hardening** — ported, reverted, then re-applied once reconciled.
- **LLVM 19.1.7 switch** — set up in CMake and intended, but the flags didn't persist in the build cache, so on-disk builds silently run **LLVM 20.1.3** (what testers actually use).
- **cellVdec re-vendor for the GoW3 Sony-intro hang** — cascades into 8+ per-codec PRX headers with little decode-loop change; reverted (needs an on-device `Vdec=Trace` repro).
</details>

## Pending / TODO for full upstream parity

<details>
<summary>Expand</summary>

- **`texture_cache` / `address_range32` migration** — the receiver texture-cache path is byte-identical to upstream but a re-vendor is gated on a fork-wide `address_range` → `address_range32` rename across all of RSX. Deferred.
- **`format_ex` / FORMAT_FEATURE (FF_*) bits** — unified port of `get_format_features` + `texture_format_ex` + replacing the inline int8 path so features gate the conversion (a naive additive fix double-applies biased-renorm to validated-good int8 textures). Bounded but needs its own on-device pass.
- **Async shader interpreter (non-blocking re-land)** — Strategy-B adapter is specced (shared_ptr cache + retire-list for the UAF, boot-only non-blocking preload). Would let smooth-shaders default ON.
- **VK 0.0.41 correctness grafts** (each with rendering-regression risk — validate, don't blind-stack): deswizzle generalization to 1-byte/wide blocks; zeta-address cyclic-ref depth-flicker barriers; SNORM/SRGB HW texel remap + `image_view::as()`; descriptor-pool autoscaling; `VK_EXT_multi_draw`; SPIR-V 1.2 / SpV 1.5 bump (gate with the shader work).
- **Clans HLE full re-vendor** — `clans_client`/`clans_config` compile but the 883→1360-line `sceNpClans.cpp` re-vendor is deferred.
- **RPCN hardening** — store `rpcn.yml` (derived password + token) in internal `filesDir` / EncryptedSharedPreferences; connection-status UI; sweep room/P2P logs for other players' NPIDs/IPs; make the offline toggle authoritative over per-game overrides + tear down live sessions.
- **GoW3 / SPURS-stall titles** — need an on-device `cellVdec` trace; not fixable from kernel code (upstream 0.0.41 did not rewrite `sys_event`).
- **SPU SVE/SVE2 codegen** — detection stubs exist, codegen does not; low value on current SoCs.
- **Snapshot catch-up** — loader `lv2_memory_container::take()` exec sizing, `ppu_check_patch_spu_images` gating, `spu_limits` check split; localized strings; perf-metrics margins; savestate `global_version` 19→21.
</details>

---

*Range `b41e09a04..HEAD`: ~250 commits across the core (+116k/−31k lines). The Demon's
Souls shadow item appears in both the "changes" and "tried/reverted" lists by design — the
final validated fix, plus the long trail of attempts that preceded it.*
