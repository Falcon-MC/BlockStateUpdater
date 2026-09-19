#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <string>

class BlockStateData {
public:
    BlockStateData();

    BlockStateData(std::string name, Tag states, int32_t version);

    static BlockStateData fromNbt(const Tag &tag);

    Tag toNbt() const;

    const std::string &getName() const {
        return mName;
    }

    const Tag &getStates() const {
        return mStates;
    }

    int32_t getVersion() const {
        return mVersion;
    }

private:
    std::string mName;
    Tag mStates;
    int32_t mVersion;
};
