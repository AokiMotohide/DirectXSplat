#include "io/archive/zip_archive.h"

#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

#include "miniz.h"

namespace dxsplat::io {

namespace {

constexpr uint64_t kMaxZipArchiveBytes = 2ull * 1024ull * 1024ull * 1024ull;
constexpr uint64_t kMaxZipEntryBytes = 512ull * 1024ull * 1024ull;
constexpr uint64_t kMaxZipTotalEntryBytes = 1024ull * 1024ull * 1024ull;
constexpr mz_uint kMaxZipEntries = 65536u;

}

Status ZipArchive::Open(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return Status::Error("failed to open zip");
  }

  file.seekg(0, std::ios::end);
  const std::streamoff end = file.tellg();
  if (end < 0 || static_cast<uint64_t>(end) > kMaxZipArchiveBytes) {
    return Status::Error("zip archive is too large");
  }
  const size_t size = static_cast<size_t>(end);
  file.seekg(0, std::ios::beg);

  try {
    bytes_.resize(size);
  } catch (const std::bad_alloc&) {
    return Status::Error("zip allocation failed");
  } catch (const std::length_error&) {
    return Status::Error("zip archive is too large");
  }
  file.read(reinterpret_cast<char*>(bytes_.data()), static_cast<std::streamsize>(size));
  if (!file) {
    return Status::Error("failed to read zip");
  }

  mz_zip_archive zip{};
  if (!mz_zip_reader_init_mem(&zip, bytes_.data(), bytes_.size(), 0)) {
    return Status::Error("failed to initialize zip reader");
  }

  entries_.clear();
  const mz_uint count = mz_zip_reader_get_num_files(&zip);
  if (count > kMaxZipEntries) {
    mz_zip_reader_end(&zip);
    return Status::Error("zip archive has too many entries");
  }
  uint64_t totalUncompressedSize = 0;
  try {
    for (mz_uint i = 0; i < count; ++i) {
      mz_zip_archive_file_stat stat{};
      if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
        mz_zip_reader_end(&zip);
        return Status::Error("failed to read zip file stat");
      }
      Entry entry{};
      entry.name = stat.m_filename;
      entry.fileIndex = i;
      entry.uncompressedSize = static_cast<uint64_t>(stat.m_uncomp_size);
      if (entry.uncompressedSize > kMaxZipEntryBytes ||
          entry.uncompressedSize > kMaxZipTotalEntryBytes - totalUncompressedSize) {
        mz_zip_reader_end(&zip);
        entries_.clear();
        return Status::Error("zip archive entries are too large");
      }
      totalUncompressedSize += entry.uncompressedSize;
      entries_.insert_or_assign(entry.name, entry);
    }
  } catch (const std::bad_alloc&) {
    mz_zip_reader_end(&zip);
    entries_.clear();
    return Status::Error("zip allocation failed");
  } catch (const std::length_error&) {
    mz_zip_reader_end(&zip);
    entries_.clear();
    return Status::Error("zip archive is too large");
  }

  mz_zip_reader_end(&zip);
  return Status::Ok();
}

bool ZipArchive::HasEntry(const std::string& name) const {
  return entries_.find(name) != entries_.end();
}

StatusOr<uint64_t> ZipArchive::EntrySize(const std::string& name) const {
  const auto it = entries_.find(name);
  if (it == entries_.end()) {
    return StatusOr<uint64_t>::Error("entry not found");
  }
  return StatusOr<uint64_t>::Ok(it->second.uncompressedSize);
}

StatusOr<std::vector<uint8_t>> ZipArchive::ReadEntry(const std::string& name) const {
  const auto it = entries_.find(name);
  if (it == entries_.end()) {
    return StatusOr<std::vector<uint8_t>>::Error("entry not found");
  }

  mz_zip_archive zip{};
  if (!mz_zip_reader_init_mem(&zip, bytes_.data(), bytes_.size(), 0)) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to initialize zip reader");
  }

  size_t uncompressedSize = 0;
  if (it->second.uncompressedSize > kMaxZipEntryBytes ||
      it->second.uncompressedSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
    mz_zip_reader_end(&zip);
    return StatusOr<std::vector<uint8_t>>::Error("zip entry is too large");
  }
  uncompressedSize = static_cast<size_t>(it->second.uncompressedSize);

  std::vector<uint8_t> out;
  if (uncompressedSize == 0) {
    mz_zip_reader_end(&zip);
    return StatusOr<std::vector<uint8_t>>::Ok(std::move(out));
  }
  try {
    out.resize(uncompressedSize);
  } catch (const std::bad_alloc&) {
    mz_zip_reader_end(&zip);
    return StatusOr<std::vector<uint8_t>>::Error("zip entry allocation failed");
  } catch (const std::length_error&) {
    mz_zip_reader_end(&zip);
    return StatusOr<std::vector<uint8_t>>::Error("zip entry is too large");
  }
  if (!mz_zip_reader_extract_to_mem(&zip, it->second.fileIndex, out.data(), out.size(), 0)) {
    mz_zip_reader_end(&zip);
    return StatusOr<std::vector<uint8_t>>::Error("failed to extract zip entry");
  }
  mz_zip_reader_end(&zip);

  return StatusOr<std::vector<uint8_t>>::Ok(std::move(out));
}

}
