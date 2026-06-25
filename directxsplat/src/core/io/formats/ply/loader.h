#pragma once

#include <string>
#include <vector>

#include "directxsplat/scene.h"
#include "directxsplat/status.h"

namespace directxsplat::io {

struct PlyLoadResult {
  GaussianSet set;
  bool wasCompressed = false;
  std::vector<std::string> warnings;
};

class PlyLoader {
 public:
  StatusOr<PlyLoadResult> Load(const std::string& path, const std::string& setName) const;
};

}

