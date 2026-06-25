#include "io/formats/ply/raw/ply_writer.h"

#include <fstream>

namespace directxsplat::io::ply {

Status WritePlyAscii(const PlyFile& file, const std::string& path) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return Status::Error("failed to open output file");
  }

  out << "ply\n";
  out << "format ascii 1.0\n";
  for (const auto& comment : file.comments) {
    out << "comment " << comment << "\n";
  }

  for (const auto& element : file.elements) {
    out << "element " << element.name << " " << element.count << "\n";
    for (const auto& property : element.properties) {
      if (property.isList) {
        out << "property list uint int " << property.name << "\n";
      } else {
        out << "property float " << property.name << "\n";
      }
    }
  }
  out << "end_header\n";

  for (const auto& element : file.elements) {
    for (uint32_t row = 0; row < element.count; ++row) {
      bool first = true;
      for (size_t i = 0; i < element.properties.size(); ++i) {
        const auto& property = element.properties[i];
        if (!first) {
          out << ' ';
        }
        first = false;
        if (property.isList) {
          out << 0;
        } else {
          const double value = (row < element.scalarColumns[i].size()) ? element.scalarColumns[i][row] : 0.0;
          out << value;
        }
      }
      out << '\n';
    }
  }

  return Status::Ok();
}

}

