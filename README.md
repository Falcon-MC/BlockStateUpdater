<p align="center">
	<picture>
		<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo-white.png">
		<img src="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo.png" alt="Falcon" width="200">
	</picture>
	<br>
	<b>Falcon Block State Updater</b>
	<br>
	Minecraft: Bedrock Edition block state upgrader written in C++17
</p>

<p align="center">
	<img src="https://img.shields.io/badge/minecraft-v1.26.51%20(Bedrock)-56383E" alt="Minecraft">
	<img src="https://img.shields.io/badge/language-C%2B%2B17-00599C" alt="C++17">
</p>

## What is this?

Worlds saved by older versions of Minecraft: Bedrock Edition store block states (name, states compound and version)
that the current game no longer uses: renamed blocks, renamed or removed properties, properties flattened into the
block name, and so on. The game only upgrades a chunk when it is loaded, so a server reading an old world has to
upgrade every state itself.

This library applies the data-driven upgrade schemas in [`Schemas/`](Schemas) to a block state and returns the state
of the newest version. It is used by [Falcon](https://github.com/Falcon-MC/Falcon) to load old worlds.

- **`BlockStateData`** - a block state: name, states compound and version, convertible from and to the NBT compound
  stored in worlds (`name`, `states`, `version`)
- **`BlockStateUpgradeSchema`** - one schema file: renamed ids, added, removed and renamed properties, remapped
  property values, flattened properties and remapped states
- **`BlockStateUpgrader`** - loads the schemas of a directory, orders them by version and schema ID and upgrades
  states

## Upgrade rules

Schemas are grouped by version ID (`major << 24 | minor << 16 | patch << 8 | revision`) and applied in version
order, then in schema ID order (the four digit file name prefix) inside a version.

- Every schema whose version is newer than the state version is applied.
- Several schemas may share a version ID because the game does not always bump it. When a version has more than one
  schema, all of them are applied even if the state has that exact version, because the version alone does not tell
  which of them were already applied. A version with a single schema is skipped when the state has that version.
- Inside a schema, `remappedStates` has priority: the first rule of the block whose `oldState` filter matches the
  state replaces the name and states, and no other rule of that schema is applied.
- Otherwise the block is renamed (`renamedIds`) or flattened (`flattenedProperties`), then `addedProperties`,
  `removedProperties`, `renamedProperties` and `remappedPropertyValues` are applied. All of them are indexed by the
  old block name and the old property names.
- The upgraded state always carries the newest schema version.

Old states stored as a legacy numeric id and meta value are not handled; they need the id and meta mapping tables
before they can be upgraded.

## Usage

The library is a plain CMake target named `FalconBlockStateUpdater`. It depends on
[Falcon NBT](https://github.com/Falcon-MC/NBT), which is fetched automatically. The CMake variable
`FALCON_BLOCK_UPGRADE_SCHEMA_DIR` points to the schema directory.

```cmake
include(FetchContent)

FetchContent_Declare(
    falcon_block_state_updater
    GIT_REPOSITORY https://github.com/Falcon-MC/BlockStateUpdater.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(falcon_block_state_updater)

target_link_libraries(your_target PRIVATE FalconBlockStateUpdater)
```

```cpp
#include "Core/BlockState/BlockStateUpgrader.h"

BlockStateUpgrader upgrader(BlockStateUpgrader::loadSchemas("Schemas"));

Tag states = Tag::ofCompound();
states.putString("color", "red");

BlockStateData upgraded = upgrader.upgrade(BlockStateData("minecraft:wool", states, 17825806));
```

## Building

Only CMake 3.16+ and a C++17 compiler are required. To build against a local Falcon NBT checkout instead of the
GitHub repository, set `FALCON_NBT_SOURCE_DIR`:

```
cmake -B build -G Ninja -DFALCON_NBT_SOURCE_DIR=../Falcon-NBT
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

The code is LGPL-3.0, see [LICENSE](LICENSE) and [COPYING](COPYING). The schema files are CC0 1.0, see
[Schemas/LICENSE](Schemas/LICENSE).
