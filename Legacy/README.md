# Legacy tables

Worlds saved before 1.12 store blocks as a numeric id and a meta value. These two files turn them into 1.12
states, which `BlockStateUpgrader` then brings up to date.

- `block_legacy_id_map.json`: numeric id to block name.
- `id_meta_1.12.0.bin`: for each block name, every meta value the block had and its 1.12 state. Layout: an
  unsigned varint count of blocks, then for each block its name (varint length and bytes), a varint count of
  metas, and for each meta the varint meta followed by the state as a little-endian NBT compound.

Both come from [pmmp/BedrockBlockUpgradeSchema](https://github.com/pmmp/BedrockBlockUpgradeSchema), which
generates them from the dedicated server, and are CC0 1.0. One entry was added: id 230, `minecraft:chalkboard`,
which the 1.26.60 client still maps.

`Tests/LegacyBlockStatesTests.cpp` checks every id from 0 to 255 against the map read from the 1.26.60 client:
each one must give the same block as the client with meta 0.
