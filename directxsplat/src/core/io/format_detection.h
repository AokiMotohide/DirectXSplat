#pragma once

#include <string>

#include "directxsplat/io.h"

namespace directxsplat::io {

SceneFormat DetectSceneFormat(const std::string& path);

}

