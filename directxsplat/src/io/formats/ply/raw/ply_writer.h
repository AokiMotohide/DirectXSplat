#pragma once

#include <string>

#include "io/formats/ply/raw/ply_types.h"

namespace dxsplat::io::ply {

Status WritePlyAscii(const PlyFile& file, const std::string& path);

}

