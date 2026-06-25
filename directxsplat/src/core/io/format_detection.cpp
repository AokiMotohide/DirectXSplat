#include "io/format_detection.h"

#include <algorithm>
#include <cctype>
#include <exception>

#include "filesystem.hpp"

namespace directxsplat::io {

namespace fs = ghc::filesystem;

namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
  if (s.size() < suffix.size()) {
    return false;
  }
  return s.substr(s.size() - suffix.size()) == suffix;
}

}  // namespace

SceneFormat DetectSceneFormat(const std::string& path) try {
  const fs::path p(path);
  if (fs::exists(p) && fs::is_directory(p)) {
    if (fs::exists(p / "lod-meta.json")) {
      return SceneFormat::HierarchicalLod;
    }
    if (fs::exists(p / "meta.json")) {
      return SceneFormat::Sog;
    }
  }
  const std::string lower = ToLower(path);
  if (EndsWith(lower, "lod-meta.json")) {
    return SceneFormat::HierarchicalLod;
  }
  if (EndsWith(lower, ".compressed.ply")) {
    return SceneFormat::CompressedPly;
  }
  if (EndsWith(lower, ".ply")) {
    return SceneFormat::Ply;
  }
  if (EndsWith(lower, ".sog") || EndsWith(lower, "meta.json")) {
    return SceneFormat::Sog;
  }
  if (EndsWith(lower, ".spz")) {
    return SceneFormat::Spz;
  }
  if (EndsWith(lower, ".splat")) {
    return SceneFormat::Splat;
  }
  return SceneFormat::Unknown;
} catch (const fs::filesystem_error&) {
  return SceneFormat::Unknown;
} catch (const std::exception&) {
  return SceneFormat::Unknown;
}

}
