#pragma once

#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace openrepublic::economy {

inline constexpr std::size_t MaxCheckpointCatalogBytes = 1024 * 1024;
inline constexpr std::size_t MaxCheckpointBytes = 2 * 1024 * 1024 + 128;

/** Owns the catalog and world restored from one checkpoint.
 *  Member order keeps the catalog alive until after the world is destroyed. */
struct RestoredCheckpoint {
    RestoredCheckpoint() = default;
    RestoredCheckpoint(const RestoredCheckpoint &) = delete;
    RestoredCheckpoint &operator=(const RestoredCheckpoint &) = delete;
    RestoredCheckpoint(RestoredCheckpoint &&) noexcept = default;
    RestoredCheckpoint &operator=(RestoredCheckpoint &&other) noexcept {
        if (this != &other) {
            world.reset();
            catalog.reset();
            catalog = std::move(other.catalog);
            world = std::move(other.world);
        }
        return *this;
    }

    std::unique_ptr<Catalog> catalog;
    std::unique_ptr<World> world;
};

/** Serializes a canonical catalog with its native world save in a bounded checkpoint. */
bool SaveCheckpoint(const World &world, std::string &outText, ParseError &error);

/** Restores the checkpoint's embedded catalog and world transactionally.
 *  The embedded catalog is authoritative for this operation. */
bool RestoreCheckpoint(std::string_view text, RestoredCheckpoint &out, ParseError &error);

/** Loads checkpoint world state only when its canonical catalog exactly matches expected. */
bool LoadCheckpoint(std::string_view text, const Catalog &expected, World &out, ParseError &error);

} // namespace openrepublic::economy
