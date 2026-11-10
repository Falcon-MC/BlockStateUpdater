#pragma once

#include "Core/BlockState/BlockStateData.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// the block states that worlds older than 1.12 stored as a numeric id and a
// meta value. Built from Legacy/block_legacy_id_map.json and
// Legacy/id_meta_1.12.0.bin; the states it returns are 1.12 states, still to be
// passed through BlockStateUpgrader
class LegacyBlockStates {
public:
  LegacyBlockStates() = default;

  // the contents of the two files, as read from disk or embedded
  LegacyBlockStates(const std::string &idMapJson,
                    const std::string &idMetaBinary);

  static LegacyBlockStates load(const std::string &directory);

  bool empty() const { return mStates.empty(); }

  // the name of a numeric id, empty when the id is not known
  const std::string &nameOf(int32_t id) const;

  // the 1.12 state of an id and meta. A meta the block never had falls back to
  // meta 0, an unknown id to air
  BlockStateData fromLegacy(int32_t id, int32_t meta) const;

private:
  std::unordered_map<int32_t, std::string> mNames;
  std::unordered_map<std::string, std::unordered_map<int32_t, BlockStateData>>
      mStates;
};
