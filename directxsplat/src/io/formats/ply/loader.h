#pragma once

#include <string>
#include <vector>

#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat::io {

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

