# rpcsx — Ouroboros (experimental)

The **PlayStation 3** emulation core (RPCS3-derived) used by the
[Android app](https://github.com/Ouroboros420/rpcsx-ui-android), built into
`librpcsx-android.so` for arm64. A continuously-updated fork of
[RPCSX/rpcsx](https://github.com/RPCSX/rpcsx).

### What's different here
Newer changes from upstream [RPCS3](https://github.com/RPCS3/rpcs3) are merged
in ahead of RPCSX's own vendoring — e.g. the SPU LLVM **Reduced Loop**, the
authoritative ARM64 NEON recompiler work — plus ARM/Android fixes. See
[`CHANGELOG.md`](CHANGELOG.md) and [`PORTING.md`](PORTING.md).

### Honest note on how it's made
The work of merging/porting these RPCS3 changes into this restructured tree is
done with heavy **AI assistance** (Claude). Treat it as **experimental** —
verify before relying on it, and expect rough edges.

### Why this exists
Not to splinter off yet another permanent fork of the PS3 emulator. The goals
are simply to **keep this build current**, and to **help the real RPCSX / RPCS3
teams** by demonstrating what's possible — they are welcome to take any idea or
change from here. All credit for the emulator belongs to the **RPCS3** and
**RPCSX** developers.

> Piracy is not permitted. Do not ask for games or system files.
