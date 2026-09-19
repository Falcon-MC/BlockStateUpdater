#include "Core/BlockState/BlockStateData.h"

#include "Core/BlockState/BlockStateUpgradeSchema.h"

BlockStateData::BlockStateData() : mStates(Tag::ofCompound()), mVersion(0) {
}

BlockStateData::BlockStateData(std::string name, Tag states, int32_t version)
        : mName(std::move(name)), mStates(std::move(states)), mVersion(version) {
    if (!mStates.isCompound())
        throw BlockStateUpgradeException("Block states must be a compound tag");
}

BlockStateData BlockStateData::fromNbt(const Tag &tag) {
    if (!tag.contains("name", Tag::Type::String))
        throw BlockStateUpgradeException("Block state is missing its name");

    const Tag *states = tag.get("states");
    if (states != nullptr && !states->isCompound())
        throw BlockStateUpgradeException("Block state \"states\" must be a compound tag");

    return BlockStateData(tag.getString("name"), states != nullptr ? *states : Tag::ofCompound(),
                          tag.getInt("version"));
}

Tag BlockStateData::toNbt() const {
    Tag tag = Tag::ofCompound();
    tag.putString("name", mName);
    tag.put("states", mStates);
    tag.putInt("version", mVersion);
    return tag;
}
