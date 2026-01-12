# Oasis Racing Game

![Racing Game](https://github.com/refuge-studios/oasis-racing-game/blob/main/.github/image.png)

A simple example game demonstrating how to build and publish a game module using the **Oasis Game API**.

This project is intended as a **reference implementation** for developers creating games on the Oasis platform.

---

## Overview

**Oasis Racing Game** shows:

* How to structure a game module for Oasis
* How to integrate with the Oasis Game API
* How to build a shared game module (`.so / .dll / .dylib`)
* How to publish a game using the Oasis CLI

It is intentionally minimal and focuses on **clarity over features**.

---

## Requirements

* C++20 compatible compiler
* CMake ≥ 3.20
* Oasis Engine headers / SDK
* Oasis CLI

👉 Oasis CLI repository:
[https://github.com/refuge-studios/oasis-cli](https://github.com/refuge-studios/oasis-cli)

---

## Project Structure

```
racing-game/
├─ assets/
│  ├─ car.svdag
│  └─ track.svdag
├─ build/
│  └─ game_module.so    # Game Module
├─ src/
│  └─ main.cpp          # Game logic entry point
├─ include/             # Public headers
├─ CMakeLists.txt
├─ game.json
├─ manifest.json
└─ README.md
```

Build output will produce a game module:

```
game_module.so   (Linux)
game_module.dll  (Windows)
game_module.dylib (macOS)
```

---

## Building the Game Module

```bash
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Publishing with Oasis CLI

1. **Initialize a project**

```bash
oasis init racing-game
cd racing-game
```

2. **Copy or build the module**

Ensure your compiled module matches the path in `game.json`:

```json
{
  "game_id": "racing-game",
  "module": "build/game_module.so",
  "assets": "assets"
}
```

3. **Publish**

```bash
oasis publish
```

For a dry run:

```bash
oasis publish --dry-run
```

---

## What This Example Demonstrates

* Game module lifecycle
* Engine ↔ game boundary
* Minimal API usage
* Incremental publishing via manifest hashing

---

## Intended Audience

This project is for:

* Developers new to Oasis
* Engine contributors
* Anyone wanting a minimal starting point for an Oasis game

It is **not** intended to be a full game.

---

## Related Projects

* **Oasis CLI**
  [https://github.com/refuge-studios/oasis-cli](https://github.com/refuge-studios/oasis-cli)