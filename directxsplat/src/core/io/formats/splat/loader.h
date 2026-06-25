#pragma once

#include <string>

#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace directxsplat::io {

class SplatLoader {
 public:
  StatusOr<GaussianSet> Load(const std::string& path, const std::string& setName) const;
};

}

