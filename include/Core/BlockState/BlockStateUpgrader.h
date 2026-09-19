#pragma once

#include "Core/BlockState/BlockStateData.h"
#include "Core/BlockState/BlockStateUpgradeSchema.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

class BlockStateUpgrader {
public:
    BlockStateUpgrader() = default;

    explicit BlockStateUpgrader(std::vector<BlockStateUpgradeSchema> schemas);

    static std::vector<BlockStateUpgradeSchema> loadSchemas(const std::string &directory,
                                                            int32_t maxSchemaId = std::numeric_limits<int32_t>::max());

    void addSchema(BlockStateUpgradeSchema schema);

    size_t getSchemaCount() const;

    int32_t getOutputVersion() const {
        return mOutputVersion;
    }

    BlockStateData upgrade(const BlockStateData &blockStateData) const;

private:
    std::pair<std::string, Tag> _applySchema(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                             const Tag &states) const;

    bool _applyStateRemapped(const BlockStateUpgradeSchema &schema, const std::string &oldName, const Tag &oldState,
                             std::pair<std::string, Tag> &outResult) const;

    std::pair<std::string, Tag> _applyPropertyFlattened(const BlockStateUpgradeSchemaFlattenRule &rule,
                                                        const std::string &oldName, const Tag &states) const;

    void _applyPropertyAdded(const BlockStateUpgradeSchema &schema, const std::string &oldName, Tag &states) const;

    void _applyPropertyRemoved(const BlockStateUpgradeSchema &schema, const std::string &oldName, Tag &states) const;

    void _applyPropertyRenamedOrValueChanged(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                             Tag &states) const;

    void _applyPropertyValueChanged(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                    Tag &states) const;

    Tag _locateNewPropertyValue(const BlockStateUpgradeSchema &schema, const std::string &oldName,
                                const std::string &oldPropertyName, const Tag &oldValue) const;

    std::map<int32_t, std::map<int32_t, BlockStateUpgradeSchema>> mSchemas;
    int32_t mOutputVersion = 0;
};
