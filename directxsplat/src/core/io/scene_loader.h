#pragma once

#include <string>

#include "dxsplat/io.h"

namespace directxsplat::io {

class SceneLoader {
 public:
  StatusOr<Scene> Load(const std::string& path, const SceneLoadOptions& options = {}) const;
};

}

