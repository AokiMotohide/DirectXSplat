#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat::io::ply {

enum class PlyFormat {
  Ascii,
  BinaryLittleEndian,
};

enum class PlyScalarType {
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Float32,
  Float64,
  Unknown,
};

struct PlyProperty {
  std::string name;
  PlyScalarType type = PlyScalarType::Unknown;
  bool isList = false;
  PlyScalarType listCountType = PlyScalarType::Unknown;
  PlyScalarType listValueType = PlyScalarType::Unknown;
};

struct PlyElement {
  std::string name;
  uint32_t count = 0;
  std::vector<PlyProperty> properties;
  std::vector<std::vector<double>> scalarColumns;
};

struct PlyFile {
  PlyFormat format = PlyFormat::BinaryLittleEndian;
  std::vector<std::string> comments;
  std::vector<PlyElement> elements;
};

size_t ScalarTypeSize(PlyScalarType type);
PlyScalarType ParseScalarType(const std::string& token);

}

