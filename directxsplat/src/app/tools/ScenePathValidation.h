#pragma once

#include <filesystem>

#include "directxsplat/status.h"

namespace directxsplat::internal {

StatusOr<std::filesystem::path> ValidateDroppedScenePath(const std::filesystem::path& path);

}  // namespace directxsplat::internal
