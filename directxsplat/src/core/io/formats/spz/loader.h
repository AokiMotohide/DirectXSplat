#pragma once

#include <string>

#include "directxsplat/scene.h"
#include "directxsplat/status.h"

namespace directxsplat::io {

class SpzLoader {
 public:
  StatusOr<GaussianSet> Load(const std::string& path, const std::string& setName) const;
};

}

