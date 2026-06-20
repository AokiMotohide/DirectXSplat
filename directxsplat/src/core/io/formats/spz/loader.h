#pragma once

#include <string>

#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat::io {

class SpzLoader {
 public:
  StatusOr<GaussianSet> Load(const std::string& path, const std::string& setName) const;
};

}

