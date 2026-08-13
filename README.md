# X-Ray (standalone)

Standalone Levi Launchroid/Preloader Android mod built from the supplied `libminecraftpe.so` and BedrockTools signature reference.

It hooks the six block-face tessellation functions and renders only vanilla ore blocks. This includes Overworld ores, deepslate ores, Nether Gold Ore, Nether Quartz Ore, and Ancient Debris.

BedrockTools is **not linked** and is not required at runtime.

## Build

The GitHub Actions workflow builds an ARM64 `.so` using Android NDK 27.2 and xmake.
