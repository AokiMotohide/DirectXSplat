#pragma once

#include <string>

#include "io/formats/ply/raw/ply_types.h"

namespace dxsplat::io::ply {

StatusOr<PlyFile> ReadPlyFile(const std::string& path);

const PlyElement* FindElement(const PlyFile& file, const std::string& name);
int FindScalarProperty(const PlyElement& element, const std::string& name);

}

