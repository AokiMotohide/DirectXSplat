#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat::internal {

struct CliOptions {
  std::optional<std::string> scenePath;
  std::optional<std::string> folderTraversalPath;
  std::optional<std::string> imagePathOverride;
  std::optional<uint32_t> renderWidthOverride;
  std::optional<uint32_t> renderHeightOverride;
  bool showHelp = false;
};

StatusOr<CliOptions> ParseCliOptions(const std::vector<std::string>& args);

}  // namespace dxsplat::internal
