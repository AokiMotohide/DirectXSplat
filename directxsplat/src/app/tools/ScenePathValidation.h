#pragma once

#include <filesystem>

#include "dxsplat/status.h"

namespace dxsplat::internal {

StatusOr<std::filesystem::path> ValidateDroppedScenePath(const std::filesystem::path& path);

}  // namespace dxsplat::internal
