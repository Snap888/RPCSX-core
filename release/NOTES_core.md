RPCSX-android core build. Update via the in-app core updater, or download the
`.so` for your device and install it manually.

## Highlights

- **Shutdown hang fixed.** Closing a game — or force-closing after a freeze —
  no longer leaves the app stuck. The internal "Emulation Join Thread" was
  deadlocking by joining itself; deferred shutdown work now runs on the
  main-thread dispatcher instead of inline on the worker thread.

- **Frozen intro movies (FMVs) fixed in some games.** The PAMF demuxer was
  re-vendored to the upstream SPU-accurate implementation, which carries the
  fix for demux events firing before the game has finished initializing FMV
  playback. The upstream fix is named for **White Knight Chronicles II**; the
  same movie-end / movie-start stall also affected **Disgaea 3** and
  **Disgaea D2** intros.

## Media stack

- Full demux -> decode subsystem brought up to current upstream:
  `cellDmuxPamf` (now SPU-accurate, HLE by default), `cellDmux`, `cellPamf`,
  `cellVdec`, `cellAdec`, `cellAtracXdec`, plus the new codec stub headers.

## Other changes since v2026.06.07-1950

- PPU reservation priority over SPUs (reduces livelock-style stalls).
- cellAudio `_mxr000` surmixer event-queue fixes.
- Core now reports each patch's game titles, enabling patch search by game
  name in the app's Patch Manager.
- Earlier module updates: cellVideoOut, cellMsgDialog / cellOskDialog /
  cellUserInfo / cellAudioOut, cellRec, sceNp*.

## Variants

- `librpcsx-android-arm64-v8a-armv8-a.so` — baseline; runs on every arm64 device.
- `librpcsx-android-arm64-v8a-armv8.2-a.so` — optimized (dotprod / LSE);
  auto-selected by the app on capable devices.

## Note for testers

The new demuxer is SPU-accurate — more correct (the reason the FMVs unfreeze),
but slightly heavier during movie playback. Please confirm whether
WKC2 / Disgaea 3 / Disgaea D2 now play through their intros, and watch movie
performance.

Credits: FMV fix by capriots (RPCS3 72b872df6); PPU reservation priority and
cellAudio `_mxr000` fixes by Elad (RPCS3). Ported to ARM64/Android.
