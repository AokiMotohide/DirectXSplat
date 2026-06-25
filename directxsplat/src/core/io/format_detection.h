#pragma once

#include <string>

#include "dxsplat/io.h"

namespace directxsplat::io {

SceneFormat DetectSceneFormat(const std::string& path);

}

