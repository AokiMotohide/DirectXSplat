#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat::io {

class ZipArchive {
 public:
  struct Entry {
    std::string name;
    uint32_t fileIndex = 0;
    uint64_t uncompressedSize = 0;
  };

  Status Open(const std::string& path);
  bool HasEntry(const std::string& name) const;
  StatusOr<uint64_t> EntrySize(const std::string& name) const;
  StatusOr<std::vector<uint8_t>> ReadEntry(const std::string& name) const;

 private:
  std::vector<uint8_t> bytes_;
  std::unordered_map<std::string, Entry> entries_;
};

}

