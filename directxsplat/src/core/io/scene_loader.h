#pragma once

#include <string>

#include "dxsplat/io.h"

namespace dxsplat::io {

class SceneLoader {
 public:
  StatusOr<Scene> Load(const std::string& path, const SceneLoadOptions& options = {}) const;
};

}

