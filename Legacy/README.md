# Legacy tables

Worlds saved before 1.12 store blocks as a numeric id and a meta value. The first two files turn them into 1.12
states, which `BlockStateUpgrader` then brings up to date. Items and entities were numbered too.

- `block_legacy_id_map.json`: numeric id to block name.
- `item_legacy_id_map.json`: item name to numeric item id, blocks included (ids under 256).
- `entity_id_map.json`: entity identifier to numeric entity type, the low byte of the game's actor type.
- `id_meta_1.12.0.bin`: for each block name, every meta value the block had and its 1.12 state. Layout: an
  unsigned varint count of blocks, then for each block its name (varint length and bytes), a varint count of
  metas, and for each meta the varint meta followed by the state as a little-endian NBT compound.

The block tables come from [pmmp/BedrockBlockUpgradeSchema](https://github.com/pmmp/BedrockBlockUpgradeSchema),
the item map from [pmmp/BedrockItemUpgradeSchema](https://github.com/pmmp/BedrockItemUpgradeSchema) and the
entity map from [pmmp/BedrockData](https://github.com/pmmp/BedrockData). They are generated from the dedicated
server and are CC0 1.0. One entry was added: id 230, `minecraft:chalkboard`,
which the 1.26.60 client still maps.

`Tests/LegacyBlockStatesTests.cpp` checks every id from 0 to 255 against the map read from the 1.26.60 client:
each one must give the same block as the client with meta 0.
