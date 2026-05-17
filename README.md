# 🎨 DrawGame — Multiplayer Drawing Canvas

A networked drawing application built with C++ and OpenGL. Draw together in real-time!

![License](https://img.shields.io/github/license/Quviry/DrawGame)
![Release](https://img.shields.io/github/v/release/Quviry/DrawGame)
![Build](https://img.shields.io/github/actions/workflow/status/Quviry/DrawGame/release.yml)

## Features

- 🖌️ Draw on a shared canvas with customizable brush
- 🎨 12-color palette + eyedropper tool
- 🌐 Local and networked multiplayer
- 💾 Auto-save on exit, auto-load on start
- 📦 No external runtime dependencies

## Download

Get the latest release for your platform from [Releases](../../releases).

| Platform | File |
|----------|------|
| Windows 10+ | `DrawGame-windows-x64.zip` |
| Ubuntu 20.04+ | `DrawGame-linux-x64.tar.gz` |
| macOS 11+ (Intel & Apple Silicon) | `DrawGame-macOS-universal.dmg` |

## Usage

```bash
# Single player (local)
./DrawGame

# Host for others to join
./DrawGame --open --port 7777

# Join someone's session
./DrawGame --connect 192.168.1.100 7777
```

## Controls

| Input | Action |
|-------|--------|
| Left Mouse | Draw |
| Right Mouse | Pick color (eyedropper) |
| `+` / `-` | Increase / decrease brush size |
| `Ctrl+C` | Clear canvas |
| `Ctrl+S` | Export canvas to BMP |
| `Escape` | Quit |

## Building from Source

### Requirements
- CMake 3.16+
- C++17 compiler
- OpenGL development libraries

### Linux (Ubuntu/Debian)
```bash
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### macOS
```bash
brew install cmake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Windows (MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Architecture

```
Client (OpenGL + GLFW)          Server (TCP)
┌─────────────────┐         ┌──────────────────┐
│  Renderer       │         │  Canvas (master) │
│  Local Canvas   │◄───────►│  Client Manager  │
│  Input Handler  │  TCP    │  Broadcast       │
│  Network Thread │         │  Save/Load BMP   │
└─────────────────┘         └──────────────────┘
```

Local mode runs both client and server in the same process.

## License

UwU
