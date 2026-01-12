# Oasis Racing Game

![Racing Game](https://github.com/refuge-studios/oasis-racing-game/blob/main/.github/image.png)

A simple example game demonstrating how to build and publish a game module using the **Oasis Game API**.

This project serves as a **reference implementation** for developers creating games on the Oasis platform.

---

## Overview

**Oasis Racing Game** demonstrates:

* Structuring a game module for Oasis
* Integrating with the Oasis Game API
* Building a platform-specific shared game module (`.so / .dll / .dylib`)
* Publishing a game using **Oasis CLI** with incremental asset/module uploads

It is intentionally minimal and focuses on **clarity over features**.

---

## Requirements

* C++20 compatible compiler
* CMake ≥ 3.20
* Oasis Engine headers / SDK
* Oasis CLI

Oasis CLI repository: [https://github.com/refuge-studios/oasis-cli](https://github.com/refuge-studios/oasis-cli)

---

## Project Structure

```
racing-game/
├─ assets/
│  ├─ car.svdag
│  └─ track.svdag
├─ bin/
│  ├─ linux/
│  │  └─ game_module.so
│  ├─ windows/
│  │  └─ game_module.dll
│  └─ macos/
│     └─ game_module.dylib
├─ src/
│  └─ main.cpp          # Game logic entry point
├─ include/             # Public headers
├─ CMakeLists.txt
├─ game.json
├─ manifest.json
└─ README.md
```

**Manifest example (`manifest.json`):**

```json
{
    "game_id": "racing-game",
    "name": "Racing Game",
    "version": "0.1.0",
    "modules": {
        "linux": "bin/linux/game_module.so",
        "macos": "bin/macos/game_module.dylib",
        "windows": "bin/windows/game_module.dll"
    },
    "assets": [
        { "path": "assets/car.svdag", "sha256": "SHA256_HASH_CAR", "size": 12345 },
        { "path": "assets/track.svdag", "sha256": "SHA256_HASH_TRACK", "size": 67890 }
    ]
}
```

---

## Building the Game Module

```bash
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Ensure the output matches the platform-specific paths in `manifest.json`.

---

## Publishing with Oasis CLI

1. **Initialize a project**

```bash
oasis init racing-game
cd racing-game
```

2. **Copy or build the module**

Make sure your compiled module matches the manifest's platform-specific paths.

3. **Publish**

```bash
oasis publish
```

For a dry run (shows what would be uploaded without sending files):

```bash
oasis publish --dry-run
```

Oasis CLI will automatically:

* Upload changed modules and assets
* Skip unchanged files using SHA256 hashes in `manifest.json`
* Update the manifest on the server

---

## What This Example Demonstrates

* Game module lifecycle
* Engine ↔ game boundary
* Minimal API usage
* Incremental publishing with platform-specific modules and asset hashes

---

## Intended Audience

* Developers new to Oasis
* Engine contributors
* Anyone needing a minimal starting point for an Oasis game

> Note: This project is **not a full game**, just a reference example.

---

## Related Projects

* **Oasis CLI** – [https://github.com/refuge-studios/oasis-cli](https://github.com/refuge-studios/oasis-cli)