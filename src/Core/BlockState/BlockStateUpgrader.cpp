#include "Core/BlockState/BlockStateUpgrader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

    bool parseSchemaId(const std::string &fileName, int32_t &outSchemaId) {
        if (fileName.size() < 9 || fileName.compare(fileName.size() - 5, 5, ".json") != 0)
            return false;

        for (size_t i = 0; i < 4; i++) {
            if (!std::isdigit((unsigned char) fileName[i]))
                return false;
        }

        outSchemaId = std::stoi(fileName.substr(0, 4));
        return true;
    }

    std::string readFile(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw BlockStateUpgradeException("Unable to open schema file " + path.string());

        std::ostringstream contents;
        contents << stream.rdbuf();
        return contents.str();
    }

}

BlockStateUpgrader::BlockStateUpgrader(std::vector<BlockStateUpgradeSchema> schemas) {
    for (BlockStateUpgradeSchema &schema: schemas)
        addSchema(std::move(schema));
}

std::vector<BlockStateUpgradeSchema> BlockStateUpgrader::loadSchemas(const std::string &directory,
                                                                     int32_t maxSchemaId) {
    std::map<int32_t, std::filesystem::path> files;

    for (const auto &entry: std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;

        int32_t schemaId = 0;
        if (!parseSchemaId(entry.path().filename().string(), schemaId) || schemaId > maxSchemaId)
            continue;

        if (files.count(schemaId) != 0)
            throw BlockStateUpgradeException("Two schema files use the schema ID " + std::to_string(schemaId));

        files[schemaId] = entry.path();
    }

    std::vector<BlockStateUpgradeSchema> schemas;
    schemas.reserve(files.size());

    for (const auto &file: files) {
        try {
            schemas.push_back(BlockStateUpgradeSchema::fromJson(readFile(file.second), file.first));
        } catch (const BlockStateUpgradeException &exception) {
            throw BlockStateUpgradeException("Loading schema file " + file.second.string() + ": " +
                                             exception.what());
        }
    }

    return schemas;
}

void BlockStateUpgrader::addSchema(BlockStateUpgradeSchema schema) {
    const int32_t versionId = schema.getVersionId();
    const int32_t schemaId = schema.getSchemaId();

    std::map<int32_t, BlockStateUpgradeSchema> &list = mSchemas[versionId];
    if (list.count(schemaId) != 0)
        throw BlockStateUpgradeException("Cannot add two schemas with the same schema ID and version ID");

    list.emplace(schemaId, std::move(schema));
    mOutputVersion = std::max(mOutputVersion, versionId);
}

size_t BlockStateUpgrader::getSchemaCount() const {
    size_t count = 0;

    for (const auto &version: mSchemas)
        count += version.second.size();

    return count;
}

BlockStateData BlockStateUpgrader::upgrade(const BlockStateData &blockStateData) const {
    const int32_t version = blockStateData.getVersion();
    std::string name = blockStateData.getName();
    Tag states = blockStateData.getStates();

    for (const auto &entry: mSchemas) {
        const int32_t resultVersion = entry.first;
        const std::map<int32_t, BlockStateUpgradeSchema> &schemaList = entry.second;

        if (version > resultVersion || (schemaList.size() == 1 && version == resultVersion))
            continue;

        for (const auto &schema: schemaList) {
            std::pair<std::string, Tag> result = _applySchema(schema.second, name, states);
            name = std::move(result.first);
            states = std::move(result.second);
        }
    }

    return BlockStateData(name, states, mOutputVersion);
}

std::pair<std::string, Tag> BlockStateUpgrader::_applySchema(const BlockStateUpgradeSchema &schema,
                                                             const std::string &oldName, const Tag &states) const {
    std::pair<std::string, Tag> remapped;
    if (_applyStateRemapped(schema, oldName, states, remapped))
        return remapped;

    const auto renamed = schema.mRenamedIds.find(oldName);
    const auto flattened = schema.mFlattenedProperties.find(oldName);

    if (renamed != schema.mRenamedIds.end() && flattened != schema.mFlattenedProperties.end()) {
        throw BlockStateUpgradeException("Both renamedIds and flattenedProperties are set for the same block ID \"" +
                                         oldName + "\"");
    }

    std::string newName = oldName;
    Tag newStates = states;

    if (renamed != schema.mRenamedIds.end()) {
        newName = renamed->second;
    } else if (flattened != schema.mFlattenedProperties.end()) {
        std::pair<std::string, Tag> result = _applyPropertyFlattened(flattened->second, oldName, states);
        newName = std::move(result.first);
        newStates = std::move(result.second);
    }

    _applyPropertyAdded(schema, oldName, newStates);
    _applyPropertyRemoved(schema, oldName, newStates);
    _applyPropertyRenamedOrValueChanged(schema, oldName, newStates);
    _applyPropertyValueChanged(schema, oldName, newStates);

    return {newName, newStates};
}

bool BlockStateUpgrader::_applyStateRemapped(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                             const Tag &oldState, std::pair<std::string, Tag> &outResult) const {
    const auto found = schema.mRemappedStates.find(oldName);
    if (found == schema.mRemappedStates.end())
        return false;

    for (const BlockStateUpgradeSchemaBlockRemap &remap: found->second) {
        if (remap.mOldState.size() > oldState.size())
            continue;

        bool matches = true;
        for (const auto &property: remap.mOldState) {
            const Tag *value = oldState.get(property.first);

            if (value == nullptr || *value != property.second) {
                matches = false;
                break;
            }
        }

        if (!matches)
            continue;

        std::string newName = remap.mNewName;
        if (remap.mHasFlattenedName)
            newName = _applyPropertyFlattened(remap.mNewFlattenedName, oldName, oldState).first;

        Tag newState = Tag::ofCompound();
        for (const auto &property: remap.mNewState)
            newState.put(property.first, property.second);

        for (const std::string &stateName: remap.mCopiedState) {
            const Tag *value = oldState.get(stateName);

            if (value != nullptr)
                newState.put(stateName, *value);
        }

        outResult = {newName, newState};
        return true;
    }

    return false;
}

std::pair<std::string, Tag> BlockStateUpgrader::_applyPropertyFlattened(const BlockStateUpgradeSchemaFlattenRule &rule,
                                                                        const std::string &oldName,
                                                                        const Tag &states) const {
    const Tag *flattenedValue = states.get(rule.mFlattenedProperty);

    if (flattenedValue == nullptr || flattenedValue->getType() != rule.mFlattenedPropertyType)
        return {oldName, states};

    std::string embedKey;
    switch (flattenedValue->getType()) {
        case Tag::Type::String:
            embedKey = flattenedValue->asString();
            break;
        case Tag::Type::Byte:
            embedKey = std::to_string((int) flattenedValue->asByte());
            break;
        case Tag::Type::Int:
            embedKey = std::to_string(flattenedValue->asInt());
            break;
        default:
            throw BlockStateUpgradeException("Flattened property type must be string, byte or int");
    }

    const auto remapped = rule.mFlattenedValueRemaps.find(embedKey);
    const std::string &embedValue = remapped != rule.mFlattenedValueRemaps.end() ? remapped->second : embedKey;

    Tag newStates = states;
    newStates.remove(rule.mFlattenedProperty);

    return {rule.mPrefix + embedValue + rule.mSuffix, newStates};
}

void BlockStateUpgrader::_applyPropertyAdded(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                             Tag &states) const {
    const auto found = schema.mAddedProperties.find(oldName);
    if (found == schema.mAddedProperties.end())
        return;

    for (const auto &property: found->second) {
        if (!states.contains(property.first))
            states.put(property.first, property.second);
    }
}

void BlockStateUpgrader::_applyPropertyRemoved(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                               Tag &states) const {
    const auto found = schema.mRemovedProperties.find(oldName);
    if (found == schema.mRemovedProperties.end())
        return;

    for (const std::string &propertyName: found->second)
        states.remove(propertyName);
}

Tag BlockStateUpgrader::_locateNewPropertyValue(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                                const std::string &oldPropertyName, const Tag &oldValue) const {
    const auto block = schema.mRemappedPropertyValues.find(oldName);
    if (block == schema.mRemappedPropertyValues.end())
        return oldValue;

    const auto property = block->second.find(oldPropertyName);
    if (property == block->second.end())
        return oldValue;

    for (const BlockStateUpgradeSchemaValueRemap &remap: property->second) {
        if (remap.mOld == oldValue)
            return remap.mNew;
    }

    return oldValue;
}

void BlockStateUpgrader::_applyPropertyRenamedOrValueChanged(const BlockStateUpgradeSchema &schema,
                                                             const std::string &oldName, Tag &states) const {
    const auto found = schema.mRenamedProperties.find(oldName);
    if (found == schema.mRenamedProperties.end())
        return;

    for (const auto &rename: found->second) {
        const Tag *oldValue = states.get(rename.first);
        if (oldValue == nullptr)
            continue;

        const Tag value = *oldValue;
        states.remove(rename.first);
        states.put(rename.second, _locateNewPropertyValue(schema, oldName, rename.first, value));
    }
}

void BlockStateUpgrader::_applyPropertyValueChanged(const BlockStateUpgradeSchema &schema,
                                                    const std::string &oldName, Tag &states) const {
    const auto found = schema.mRemappedPropertyValues.find(oldName);
    if (found == schema.mRemappedPropertyValues.end())
        return;

    for (const auto &property: found->second) {
        const Tag *oldValue = states.get(property.first);
        if (oldValue == nullptr)
            continue;

        const Tag value = *oldValue;
        states.put(property.first, _locateNewPropertyValue(schema, oldName, property.first, value));
    }
}
