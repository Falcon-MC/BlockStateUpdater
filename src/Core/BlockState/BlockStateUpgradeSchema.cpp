#include "Core/BlockState/BlockStateUpgradeSchema.h"

#include "Core/Json/Json.h"

#include <cmath>

namespace {

    const json::Value &requireMember(const json::Value &object, const std::string &key) {
        const json::Value *value = object.get(key);
        if (value == nullptr)
            throw BlockStateUpgradeException("Missing required key \"" + key + "\"");

        return *value;
    }

    const json::Value &requireObject(const json::Value &value, const std::string &context) {
        if (!value.isObject())
            throw BlockStateUpgradeException("Expected an object for " + context);

        return value;
    }

    const json::Value &requireArray(const json::Value &value, const std::string &context) {
        if (!value.isArray())
            throw BlockStateUpgradeException("Expected an array for " + context);

        return value;
    }

    const std::string &requireString(const json::Value &value, const std::string &context) {
        if (!value.isString())
            throw BlockStateUpgradeException("Expected a string for " + context);

        return value.mString;
    }

    int32_t requireInteger(const json::Value &value, const std::string &context) {
        if (!value.isNumber() || std::floor(value.mNumber) != value.mNumber)
            throw BlockStateUpgradeException("Expected an integer for " + context);

        return (int32_t) value.mNumber;
    }

    std::map<std::string, const json::Value *> sortedMembers(const json::Value &object, const std::string &context) {
        requireObject(object, context);

        std::map<std::string, const json::Value *> members;
        for (const auto &entry: object.mObject)
            members[entry.first] = entry.second.get();

        return members;
    }

    Tag parseTag(const json::Value &value, const std::string &context) {
        requireObject(value, context);

        if (value.mObject.size() != 1)
            throw BlockStateUpgradeException("Expected exactly one of 'byte', 'int' or 'string' for " + context);

        const auto &entry = *value.mObject.begin();

        if (entry.first == "byte")
            return Tag::ofByte((int8_t) requireInteger(*entry.second, context));

        if (entry.first == "int")
            return Tag::ofInt(requireInteger(*entry.second, context));

        if (entry.first == "string")
            return Tag::ofString(requireString(*entry.second, context));

        throw BlockStateUpgradeException("Unexpected tag type \"" + entry.first + "\" for " + context);
    }

    std::vector<std::pair<std::string, Tag>> parseState(const json::Value &value, const std::string &context) {
        std::vector<std::pair<std::string, Tag>> state;

        if (value.mType == json::Value::Type::Null)
            return state;

        for (const auto &member: sortedMembers(value, context))
            state.emplace_back(member.first, parseTag(*member.second, context + "." + member.first));

        return state;
    }

    BlockStateUpgradeSchemaFlattenRule parseFlattenRule(const json::Value &value, const std::string &context) {
        static const char *const KNOWN_KEYS[] = {"prefix", "flattenedProperty", "flattenedPropertyType", "suffix",
                                                 "flattenedValueRemaps"};

        for (const auto &member: sortedMembers(value, context)) {
            bool known = false;
            for (const char *key: KNOWN_KEYS)
                known = known || member.first == key;

            if (!known)
                throw BlockStateUpgradeException("Unexpected key \"" + member.first + "\" in " + context);
        }

        BlockStateUpgradeSchemaFlattenRule rule;
        rule.mPrefix = requireString(requireMember(value, "prefix"), context + ".prefix");
        rule.mFlattenedProperty = requireString(requireMember(value, "flattenedProperty"),
                                                context + ".flattenedProperty");
        rule.mSuffix = requireString(requireMember(value, "suffix"), context + ".suffix");

        const json::Value *type = value.get("flattenedPropertyType");
        if (type != nullptr) {
            const std::string &typeName = requireString(*type, context + ".flattenedPropertyType");

            if (typeName == "string")
                rule.mFlattenedPropertyType = Tag::Type::String;
            else if (typeName == "int")
                rule.mFlattenedPropertyType = Tag::Type::Int;
            else if (typeName == "byte")
                rule.mFlattenedPropertyType = Tag::Type::Byte;
            else
                throw BlockStateUpgradeException("Unexpected flattened property type \"" + typeName + "\" in " +
                                                 context);
        }

        const json::Value *remaps = value.get("flattenedValueRemaps");
        if (remaps != nullptr) {
            for (const auto &member: sortedMembers(*remaps, context + ".flattenedValueRemaps")) {
                rule.mFlattenedValueRemaps[member.first] = requireString(
                        *member.second, context + ".flattenedValueRemaps." + member.first);
            }
        }

        return rule;
    }

}

BlockStateUpgradeSchema::BlockStateUpgradeSchema(int32_t maxVersionMajor, int32_t maxVersionMinor,
                                                 int32_t maxVersionPatch, int32_t maxVersionRevision,
                                                 int32_t schemaId)
        : mMaxVersionMajor(maxVersionMajor), mMaxVersionMinor(maxVersionMinor), mMaxVersionPatch(maxVersionPatch),
          mMaxVersionRevision(maxVersionRevision), mSchemaId(schemaId),
          mVersionId(makeVersionId(maxVersionMajor, maxVersionMinor, maxVersionPatch, maxVersionRevision)) {
}

int32_t BlockStateUpgradeSchema::makeVersionId(int32_t major, int32_t minor, int32_t patch, int32_t revision) {
    return (int32_t) (((uint32_t) major << 24) | ((uint32_t) minor << 16) | ((uint32_t) patch << 8) |
                      (uint32_t) revision);
}

bool BlockStateUpgradeSchema::isEmpty() const {
    return mRenamedIds.empty() && mAddedProperties.empty() && mRemovedProperties.empty() &&
           mRenamedProperties.empty() && mRemappedPropertyValues.empty() && mFlattenedProperties.empty() &&
           mRemappedStates.empty();
}

BlockStateUpgradeSchema BlockStateUpgradeSchema::fromJson(const std::string &source, int32_t schemaId) {
    const std::unique_ptr<json::Value> root = json::parse(source);
    if (root == nullptr)
        throw BlockStateUpgradeException("Schema is not valid JSON");

    static const char *const KNOWN_KEYS[] = {"maxVersionMajor", "maxVersionMinor", "maxVersionPatch",
                                             "maxVersionRevision", "renamedIds", "renamedProperties",
                                             "addedProperties", "removedProperties", "remappedPropertyValues",
                                             "remappedPropertyValuesIndex", "flattenedProperties", "remappedStates"};

    for (const auto &member: sortedMembers(*root, "schema root")) {
        bool known = false;
        for (const char *key: KNOWN_KEYS)
            known = known || member.first == key;

        if (!known)
            throw BlockStateUpgradeException("Unexpected key \"" + member.first + "\" in schema root");
    }

    BlockStateUpgradeSchema schema(requireInteger(requireMember(*root, "maxVersionMajor"), "maxVersionMajor"),
                                   requireInteger(requireMember(*root, "maxVersionMinor"), "maxVersionMinor"),
                                   requireInteger(requireMember(*root, "maxVersionPatch"), "maxVersionPatch"),
                                   requireInteger(requireMember(*root, "maxVersionRevision"), "maxVersionRevision"),
                                   schemaId);

    if (const json::Value *renamedIds = root->get("renamedIds")) {
        for (const auto &block: sortedMembers(*renamedIds, "renamedIds"))
            schema.mRenamedIds[block.first] = requireString(*block.second, "renamedIds." + block.first);
    }

    if (const json::Value *renamedProperties = root->get("renamedProperties")) {
        for (const auto &block: sortedMembers(*renamedProperties, "renamedProperties")) {
            const std::string context = "renamedProperties." + block.first;

            for (const auto &property: sortedMembers(*block.second, context)) {
                schema.mRenamedProperties[block.first].emplace_back(
                        property.first, requireString(*property.second, context + "." + property.first));
            }
        }
    }

    if (const json::Value *addedProperties = root->get("addedProperties")) {
        for (const auto &block: sortedMembers(*addedProperties, "addedProperties")) {
            schema.mAddedProperties[block.first] = parseState(
                    requireObject(*block.second, "addedProperties." + block.first), "addedProperties." + block.first);
        }
    }

    if (const json::Value *removedProperties = root->get("removedProperties")) {
        for (const auto &block: sortedMembers(*removedProperties, "removedProperties")) {
            const std::string context = "removedProperties." + block.first;
            std::vector<std::string> &names = schema.mRemovedProperties[block.first];

            for (const auto &name: requireArray(*block.second, context).mArray)
                names.push_back(requireString(*name, context));
        }
    }

    std::map<std::string, std::vector<BlockStateUpgradeSchemaValueRemap>> valuesIndex;
    if (const json::Value *index = root->get("remappedPropertyValuesIndex")) {
        for (const auto &mapping: sortedMembers(*index, "remappedPropertyValuesIndex")) {
            const std::string context = "remappedPropertyValuesIndex." + mapping.first;
            std::vector<BlockStateUpgradeSchemaValueRemap> &values = valuesIndex[mapping.first];

            for (const auto &pair: requireArray(*mapping.second, context).mArray) {
                requireObject(*pair, context);

                BlockStateUpgradeSchemaValueRemap remap;
                remap.mOld = parseTag(requireMember(*pair, "old"), context + ".old");
                remap.mNew = parseTag(requireMember(*pair, "new"), context + ".new");
                values.push_back(std::move(remap));
            }
        }
    }

    if (const json::Value *remappedValues = root->get("remappedPropertyValues")) {
        for (const auto &block: sortedMembers(*remappedValues, "remappedPropertyValues")) {
            const std::string context = "remappedPropertyValues." + block.first;

            for (const auto &property: sortedMembers(*block.second, context)) {
                const std::string &key = requireString(*property.second, context + "." + property.first);
                const auto found = valuesIndex.find(key);

                if (found == valuesIndex.end())
                    throw BlockStateUpgradeException("Missing key from schema values index " + key);

                schema.mRemappedPropertyValues[block.first][property.first] = found->second;
            }
        }
    }

    if (const json::Value *flattened = root->get("flattenedProperties")) {
        for (const auto &block: sortedMembers(*flattened, "flattenedProperties")) {
            schema.mFlattenedProperties[block.first] = parseFlattenRule(*block.second,
                                                                        "flattenedProperties." + block.first);
        }
    }

    if (const json::Value *remappedStates = root->get("remappedStates")) {
        for (const auto &block: sortedMembers(*remappedStates, "remappedStates")) {
            const std::string context = "remappedStates." + block.first;
            std::vector<BlockStateUpgradeSchemaBlockRemap> &remaps = schema.mRemappedStates[block.first];

            for (const auto &entry: requireArray(*block.second, context).mArray) {
                requireObject(*entry, context);

                BlockStateUpgradeSchemaBlockRemap remap;
                remap.mOldState = parseState(requireMember(*entry, "oldState"), context + ".oldState");
                remap.mNewState = parseState(requireMember(*entry, "newState"), context + ".newState");

                const json::Value *newName = entry->get("newName");
                const json::Value *newFlattenedName = entry->get("newFlattenedName");

                if ((newName == nullptr) == (newFlattenedName == nullptr))
                    throw BlockStateUpgradeException("Expected exactly one of 'newName' or 'newFlattenedName' in " +
                                                     context);

                if (newName != nullptr) {
                    remap.mNewName = requireString(*newName, context + ".newName");
                } else {
                    remap.mHasFlattenedName = true;
                    remap.mNewFlattenedName = parseFlattenRule(*newFlattenedName, context + ".newFlattenedName");
                }

                if (const json::Value *copied = entry->get("copiedState")) {
                    for (const auto &name: requireArray(*copied, context + ".copiedState").mArray)
                        remap.mCopiedState.push_back(requireString(*name, context + ".copiedState"));
                }

                remaps.push_back(std::move(remap));
            }
        }
    }

    return schema;
}
