#include "tools/ScenePathValidation.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace dxsplat::internal {

namespace fs = std::filesystem;

namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

}  // namespace

StatusOr<fs::path> ValidateDroppedScenePath(const fs::path& path) {
  if (path.empty()) {
    return StatusOr<fs::path>::Error("dropped scene path is empty");
  }

  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    return StatusOr<fs::path>::Error("dropped path is not a supported scene file");
  }

  const std::string lower = ToLower(path.filename().string());
  if (EndsWith(lower, ".ply") || EndsWith(lower, ".sog") || EndsWith(lower, ".spz") ||
      EndsWith(lower, ".splat") || lower == "meta.json" || lower == "lod-meta.json") {
    return StatusOr<fs::path>::Ok(path);
  }

  return StatusOr<fs::path>::Error("dropped path is not a supported scene file");
}

}  // namespace dxsplat::internal
