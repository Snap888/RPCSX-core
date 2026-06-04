<div align="center">

# rpcsx (Ouroboros fork)

*The PlayStation 3 emulation core used by [RPCSX-UI-Android](https://github.com/Ouroboros420/rpcsx-ui-android), with arm64/Android performance work on top.*

</div>

> **Warning:** Do not ask for links to games or system files. Piracy is not permitted here or in the upstream projects.

## What this is

This is a **personal fork** of [RPCSX/rpcsx](https://github.com/RPCSX/rpcsx). RPCSX bundles an RPCS3-derived core for **PlayStation 3** emulation; the Android build (`android/`) compiles that core into `librpcsx-android.so`, which the [Android app](https://github.com/Ouroboros420/rpcsx-ui-android) loads at runtime to run PS3 games on arm64 devices.

This fork adds **ARM64/Android-targeted optimizations and fixes** to the PS3 (SPU/PPU) recompilers, all guarded so non-ARM builds are unchanged. See [`CHANGELOG.md`](CHANGELOG.md) for the full list, with the upstream RPCS3 commit each change is derived from.

> **Honesty note on AI involvement:** the modifications in this fork were developed with **heavy assistance from an AI coding agent (Claude)**, under human direction and review. The emulator itself is the work of the **RPCS3** and **RPCSX** teams; this fork only adds changes on top of theirs. Each ported upstream change credits its original author in the commit message.

## Building the Android core

```
cmake -B build -S android -DANDROID_ABI=arm64-v8a -DUSE_ARCH=armv8-a \
  -DANDROID_PLATFORM=android-29 -DANDROID_NDK=<ndk> \
  -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build        # -> build/librpcsx-android.so
```
Requires NDK r29 (29.0.13113456), CMake 3.31.x, Ninja, Python 3, and the submodules (`git submodule update --init --recursive`). FFmpeg and LLVM are downloaded prebuilt during configure.

## Credits

- [RPCSX](https://github.com/RPCSX) and [RPCS3](https://github.com/RPCS3/rpcs3) teams — the emulator and its PS3 core.
- Fork modifications: Ouroboros420, with AI assistance (Claude); individual ported optimizations credit their upstream authors in commit messages.

## License

See [`LICENSE`](LICENSE) and the per-directory licenses of bundled components.
