#include "Core/BlockState/BlockStateUpgrader.h"
#include "Core/NBT/NbtIo.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

    int gFailures = 0;
    int gChecks = 0;

    const int32_t VERSION_1_16_0_14 = BlockStateUpgradeSchema::makeVersionId(1, 16, 0, 14);
    const int32_t VERSION_1_19_70_15 = BlockStateUpgradeSchema::makeVersionId(1, 19, 70, 15);
    const int32_t VERSION_1_20_10_32 = BlockStateUpgradeSchema::makeVersionId(1, 20, 10, 32);
    const int32_t VERSION_1_21_60_33 = BlockStateUpgradeSchema::makeVersionId(1, 21, 60, 33);

    void check(bool condition, const std::string &name) {
        gChecks++;

        if (!condition) {
            gFailures++;
            std::printf("FAIL: %s\n", name.c_str());
        }
    }

    Tag states(std::vector<std::pair<std::string, Tag>> values) {
        Tag compound = Tag::ofCompound();

        for (auto &value: values)
            compound.put(value.first, std::move(value.second));

        return compound;
    }

    void checkUpgrade(const BlockStateUpgrader &upgrader, const std::string &label, const std::string &name,
                      Tag oldStates, int32_t version, const std::string &expectedName, const Tag &expectedStates) {
        const BlockStateData result = upgrader.upgrade(BlockStateData(name, std::move(oldStates), version));

        check(result.getName() == expectedName, label + ": name is " + expectedName + ", got " + result.getName());
        check(result.getVersion() == VERSION_1_21_60_33, label + ": version is the newest schema version");
        check(result.getStates().size() == expectedStates.size(),
              label + ": expected states " + expectedStates.toString() + ", got " + result.getStates().toString());

        for (size_t i = 0; i < expectedStates.getKeys().size(); i++) {
            const std::string &key = expectedStates.getKeys()[i];
            const Tag *actual = result.getStates().get(key);

            check(actual != nullptr && *actual == expectedStates.getValues()[i],
                  label + ": state " + key + " is " + expectedStates.getValues()[i].toString());
        }
    }

    void testLoading(const BlockStateUpgrader &upgrader, size_t schemaFileCount) {
        check(schemaFileCount == 36, "36 schema files loaded");
        check(upgrader.getSchemaCount() == 36, "36 schemas registered");

        const std::vector<BlockStateUpgradeSchema> limited = BlockStateUpgrader::loadSchemas(
                FALCON_BLOCK_UPGRADE_SCHEMA_DIR, 11);
        check(limited.size() == 2, "maximum schema ID keeps schemas 0001 and 0011");
        check(upgrader.getOutputVersion() == VERSION_1_21_60_33, "output version is 1.21.60.33");

        bool threw = false;
        try {
            BlockStateUpgradeSchema::fromJson("{\"maxVersionMajor\":1}", 1);
        } catch (const BlockStateUpgradeException &) {
            threw = true;
        }
        check(threw, "schema without the required version keys is rejected");

        threw = false;
        try {
            BlockStateUpgradeSchema::fromJson("{\"maxVersionMajor\":1,\"maxVersionMinor\":0,\"maxVersionPatch\":0,"
                                              "\"maxVersionRevision\":0,\"unknownKey\":{}}", 1);
        } catch (const BlockStateUpgradeException &) {
            threw = true;
        }
        check(threw, "schema with an unknown key is rejected");
    }

    void testFlattenedProperties(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "wool red", "minecraft:wool", states({{"color", Tag::ofString("red")}}),
                     VERSION_1_16_0_14, "minecraft:red_wool", Tag::ofCompound());

        checkUpgrade(upgrader, "wool silver", "minecraft:wool", states({{"color", Tag::ofString("silver")}}),
                     VERSION_1_16_0_14, "minecraft:light_gray_wool", Tag::ofCompound());

        checkUpgrade(upgrader, "log spruce", "minecraft:log",
                     states({{"old_log_type", Tag::ofString("spruce")}, {"pillar_axis", Tag::ofString("y")}}),
                     VERSION_1_16_0_14, "minecraft:spruce_log", states({{"pillar_axis", Tag::ofString("y")}}));

        checkUpgrade(upgrader, "stonebrick default", "minecraft:stonebrick",
                     states({{"stone_brick_type", Tag::ofString("default")}}), VERSION_1_20_10_32,
                     "minecraft:stone_bricks", Tag::ofCompound());

        checkUpgrade(upgrader, "tnt underwater", "minecraft:tnt", states({{"allow_underwater_bit", Tag::ofByte(1)}}),
                     VERSION_1_20_10_32, "minecraft:underwater_tnt", Tag::ofCompound());

        checkUpgrade(upgrader, "tnt normal", "minecraft:tnt", states({{"allow_underwater_bit", Tag::ofByte(0)}}),
                     VERSION_1_20_10_32, "minecraft:tnt", Tag::ofCompound());

        checkUpgrade(upgrader, "tnt with a wrongly typed flattened property", "minecraft:tnt",
                     states({{"allow_underwater_bit", Tag::ofInt(1)}}), VERSION_1_20_10_32, "minecraft:tnt",
                     states({{"allow_underwater_bit", Tag::ofInt(1)}}));
    }

    void testLegacyStone(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "stone mapped_type 1", "minecraft:stone", states({{"mapped_type", Tag::ofInt(1)}}), 0,
                     "minecraft:granite", Tag::ofCompound());

        checkUpgrade(upgrader, "stone mapped_type 2", "minecraft:stone", states({{"mapped_type", Tag::ofInt(2)}}), 0,
                     "minecraft:polished_granite", Tag::ofCompound());

        checkUpgrade(upgrader, "stone mapped_type 0", "minecraft:stone", states({{"mapped_type", Tag::ofInt(0)}}), 0,
                     "minecraft:stone", Tag::ofCompound());
    }

    void testRenamedProperties(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "big dripleaf direction", "minecraft:big_dripleaf",
                     states({{"big_dripleaf_head", Tag::ofByte(1)},
                             {"big_dripleaf_tilt", Tag::ofString("none")},
                             {"direction", Tag::ofInt(2)}}),
                     VERSION_1_20_10_32, "minecraft:big_dripleaf",
                     states({{"big_dripleaf_head", Tag::ofByte(1)},
                             {"big_dripleaf_tilt", Tag::ofString("none")},
                             {"minecraft:cardinal_direction", Tag::ofString("north")}}));

        checkUpgrade(upgrader, "anvil very damaged", "minecraft:anvil",
                     states({{"damage", Tag::ofString("very_damaged")}, {"direction", Tag::ofInt(3)}}),
                     VERSION_1_20_10_32, "minecraft:damaged_anvil",
                     states({{"minecraft:cardinal_direction", Tag::ofString("east")}}));
    }

    void testRenamedIds(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "yellow flower", "minecraft:yellow_flower", Tag::ofCompound(), VERSION_1_20_10_32,
                     "minecraft:dandelion", Tag::ofCompound());
    }

    void testRemappedStates(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "log direction 3 birch", "minecraft:log",
                     states({{"direction", Tag::ofInt(3)}, {"old_log_type", Tag::ofString("birch")}}), 0,
                     "minecraft:birch_wood", states({{"pillar_axis", Tag::ofString("y")}}));

        checkUpgrade(upgrader, "log direction 0 oak", "minecraft:log",
                     states({{"direction", Tag::ofInt(0)}, {"old_log_type", Tag::ofString("oak")}}), 0,
                     "minecraft:oak_log", states({{"pillar_axis", Tag::ofString("y")}}));

        checkUpgrade(upgrader, "log direction 1 jungle", "minecraft:log",
                     states({{"direction", Tag::ofInt(1)}, {"old_log_type", Tag::ofString("jungle")}}), 0,
                     "minecraft:jungle_log", states({{"pillar_axis", Tag::ofString("x")}}));
    }

    void testAddedProperties(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "acacia fence connections", "minecraft:acacia_fence", Tag::ofCompound(),
                     VERSION_1_21_60_33, "minecraft:acacia_fence",
                     states({{"minecraft:connection_east", Tag::ofByte(0)},
                             {"minecraft:connection_north", Tag::ofByte(0)},
                             {"minecraft:connection_south", Tag::ofByte(0)},
                             {"minecraft:connection_west", Tag::ofByte(0)}}));

        checkUpgrade(upgrader, "acacia fence keeps existing connections", "minecraft:acacia_fence",
                     states({{"minecraft:connection_east", Tag::ofByte(1)}}), VERSION_1_21_60_33,
                     "minecraft:acacia_fence",
                     states({{"minecraft:connection_east", Tag::ofByte(1)},
                             {"minecraft:connection_north", Tag::ofByte(0)},
                             {"minecraft:connection_south", Tag::ofByte(0)},
                             {"minecraft:connection_west", Tag::ofByte(0)}}));
    }

    void testVersionHandling(const BlockStateUpgrader &upgrader) {
        checkUpgrade(upgrader, "latest state is unchanged", "minecraft:red_wool", Tag::ofCompound(),
                     VERSION_1_21_60_33, "minecraft:red_wool", Tag::ofCompound());

        checkUpgrade(upgrader, "single schema of the same version is skipped", "minecraft:wool",
                     states({{"color", Tag::ofString("red")}}), VERSION_1_19_70_15, "minecraft:wool",
                     states({{"color", Tag::ofString("red")}}));
    }

    void testNbtRoundTrip(const BlockStateUpgrader &upgrader) {
        Tag old = Tag::ofCompound();
        old.putString("name", "minecraft:wool");
        old.put("states", states({{"color", Tag::ofString("red")}}));
        old.putInt("version", VERSION_1_16_0_14);

        BinaryStream stream;
        NbtIo::writeTag(stream, old, NbtVariant::LittleEndian);

        ReadOnlyBinaryStream input(stream.getBuffer());
        const BlockStateData decoded = BlockStateData::fromNbt(NbtIo::readTag(input, NbtVariant::LittleEndian));
        const Tag upgraded = upgrader.upgrade(decoded).toNbt();

        check(upgraded.getString("name") == "minecraft:red_wool", "NBT round trip name");
        check(upgraded.get("states") != nullptr && upgraded.get("states")->isEmpty(), "NBT round trip states");
        check(upgraded.getInt("version") == VERSION_1_21_60_33, "NBT round trip version");
    }

}

int main() {
    std::vector<BlockStateUpgradeSchema> schemas = BlockStateUpgrader::loadSchemas(FALCON_BLOCK_UPGRADE_SCHEMA_DIR);
    const size_t schemaFileCount = schemas.size();

    const BlockStateUpgrader upgrader(std::move(schemas));

    testLoading(upgrader, schemaFileCount);
    testFlattenedProperties(upgrader);
    testLegacyStone(upgrader);
    testRenamedProperties(upgrader);
    testRenamedIds(upgrader);
    testRemappedStates(upgrader);
    testAddedProperties(upgrader);
    testVersionHandling(upgrader);
    testNbtRoundTrip(upgrader);

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
