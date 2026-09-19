#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class BlockStateUpgradeException : public std::runtime_error {
public:
    explicit BlockStateUpgradeException(const std::string &message) : std::runtime_error(message) {
    }
};

struct BlockStateUpgradeSchemaFlattenRule {
    std::string mPrefix;
    std::string mFlattenedProperty;
    std::string mSuffix;
    Tag::Type mFlattenedPropertyType = Tag::Type::String;
    std::map<std::string, std::string> mFlattenedValueRemaps;
};

struct BlockStateUpgradeSchemaValueRemap {
    Tag mOld;
    Tag mNew;
};

struct BlockStateUpgradeSchemaBlockRemap {
    std::vector<std::pair<std::string, Tag>> mOldState;
    bool mHasFlattenedName = false;
    std::string mNewName;
    BlockStateUpgradeSchemaFlattenRule mNewFlattenedName;
    std::vector<std::pair<std::string, Tag>> mNewState;
    std::vector<std::string> mCopiedState;
};

class BlockStateUpgradeSchema {
public:
    BlockStateUpgradeSchema(int32_t maxVersionMajor, int32_t maxVersionMinor, int32_t maxVersionPatch,
                            int32_t maxVersionRevision, int32_t schemaId);

    static BlockStateUpgradeSchema fromJson(const std::string &source, int32_t schemaId);

    static int32_t makeVersionId(int32_t major, int32_t minor, int32_t patch, int32_t revision);

    int32_t getVersionId() const {
        return mVersionId;
    }

    int32_t getSchemaId() const {
        return mSchemaId;
    }

    bool isEmpty() const;

    std::map<std::string, std::string> mRenamedIds;
    std::map<std::string, std::vector<std::pair<std::string, Tag>>> mAddedProperties;
    std::map<std::string, std::vector<std::string>> mRemovedProperties;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> mRenamedProperties;
    std::map<std::string, std::map<std::string, std::vector<BlockStateUpgradeSchemaValueRemap>>> mRemappedPropertyValues;
    std::map<std::string, BlockStateUpgradeSchemaFlattenRule> mFlattenedProperties;
    std::map<std::string, std::vector<BlockStateUpgradeSchemaBlockRemap>> mRemappedStates;

private:
    int32_t mMaxVersionMajor;
    int32_t mMaxVersionMinor;
    int32_t mMaxVersionPatch;
    int32_t mMaxVersionRevision;
    int32_t mSchemaId;
    int32_t mVersionId;
};
