#include "Core/BlockState/LegacyBlockStates.h"

#include "Core/BlockState/BlockStateUpgradeSchema.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
const char *const AIR = "minecraft:air";

std::string readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    throw BlockStateUpgradeException("Cannot open " + path);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

const std::string &emptyName() {
  static const std::string EMPTY;
  return EMPTY;
}
} // namespace

LegacyBlockStates::LegacyBlockStates(const std::string &idMapJson,
                                     const std::string &idMetaBinary) {
  const std::unique_ptr<json::Value> ids = json::parse(idMapJson);
  if (ids == nullptr || !ids->isObject())
    throw BlockStateUpgradeException("The legacy id map is not a JSON object");
  for (const auto &entry : ids->mObject) {
    if (entry.second != nullptr && entry.second->isNumber())
      mNames[(int32_t)entry.second->mNumber] = entry.first;
  }

  // count, then per block: name, count, then per meta: meta and a little-endian
  // state compound
  ReadOnlyBinaryStream stream(idMetaBinary);
  const uint32_t blocks = stream.getUnsignedVarInt();
  for (uint32_t block = 0; block < blocks && !stream.feof(); block++) {
    const uint32_t nameLength = stream.getUnsignedVarInt();
    const std::string name = stream.get(nameLength);
    std::unordered_map<int32_t, BlockStateData> &metas = mStates[name];

    const uint32_t count = stream.getUnsignedVarInt();
    for (uint32_t index = 0; index < count; index++) {
      const int32_t meta = (int32_t)stream.getUnsignedVarInt();
      metas[meta] = BlockStateData::fromNbt(
          NbtIo::readTag(stream, NbtVariant::LittleEndian));
    }
  }
}

LegacyBlockStates LegacyBlockStates::load(const std::string &directory) {
  return LegacyBlockStates(readFile(directory + "/block_legacy_id_map.json"),
                           readFile(directory + "/id_meta_1.12.0.bin"));
}

const std::string &LegacyBlockStates::nameOf(int32_t id) const {
  const auto it = mNames.find(id);
  return it == mNames.end() ? emptyName() : it->second;
}

BlockStateData LegacyBlockStates::fromLegacy(int32_t id, int32_t meta) const {
  const auto name = mNames.find(id);
  if (name == mNames.end())
    return BlockStateData(AIR, Tag::ofCompound(), 0);

  // blocks of later versions have an id but no meta table: their default state,
  // filled by the upgrader
  const auto block = mStates.find(name->second);
  if (block == mStates.end() || block->second.empty())
    return BlockStateData(name->second, Tag::ofCompound(), 0);

  auto state = block->second.find(meta);
  if (state == block->second.end())
    state = block->second.find(0);
  if (state == block->second.end())
    state = block->second.begin();
  return state->second;
}
