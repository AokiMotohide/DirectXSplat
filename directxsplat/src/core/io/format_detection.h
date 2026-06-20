#pragma once

#include <string>

#include "dxsplat/io.h"

namespace dxsplat::io {

SceneFormat DetectSceneFormat(const std::string& path);

}

