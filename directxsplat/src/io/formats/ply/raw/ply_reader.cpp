#include "io/formats/ply/raw/ply_reader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace dxsplat::io::ply {

namespace {

constexpr uint32_t kMaxPlyElements = 4096;
constexpr uint32_t kMaxPlyProperties = 4096;
constexpr uint64_t kMaxPlyFileBytes = 8ull * 1024ull * 1024ull * 1024ull;
constexpr uint64_t kMaxPlyScalarBytes = 16ull * 1024ull * 1024ull * 1024ull;
constexpr size_t kMaxPlyHeaderLineBytes = 1024ull * 1024ull;
constexpr size_t kMaxAsciiPlyTokenBytes = 4096;

std::string Trim(const std::string& s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
    ++b;
  }
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
    --e;
  }
  return s.substr(b, e - b);
}

std::vector<std::string> SplitBySpaces(const std::string& s) {
  std::vector<std::string> out;
  std::string item;
  std::istringstream in(s);
  while (in >> item) {
    out.push_back(item);
  }
  return out;
}

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

StatusOr<uint32_t> ParseUint32(const std::string& s) {
  if (s.empty() || s[0] == '-') {
    return StatusOr<uint32_t>::Error("invalid element count");
  }
  try {
    size_t consumed = 0;
    const unsigned long value = std::stoul(s, &consumed);
    if (consumed != s.size() || value > std::numeric_limits<uint32_t>::max()) {
      return StatusOr<uint32_t>::Error("invalid element count");
    }
    return StatusOr<uint32_t>::Ok(static_cast<uint32_t>(value));
  } catch (...) {
    return StatusOr<uint32_t>::Error("invalid element count");
  }
}

bool IsIntegerScalarType(PlyScalarType type) {
  return type == PlyScalarType::Int8 || type == PlyScalarType::UInt8 ||
         type == PlyScalarType::Int16 || type == PlyScalarType::UInt16 ||
         type == PlyScalarType::Int32 || type == PlyScalarType::UInt32;
}

template <typename T>
T ReadLE(const uint8_t* ptr) {
  T out{};
  std::memcpy(&out, ptr, sizeof(T));
  return out;
}

StatusOr<std::vector<uint8_t>> ReadWholeFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to open file");
  }
  file.seekg(0, std::ios::end);
  const std::streamoff end = file.tellg();
  if (end < 0) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to read file");
  }
  if (static_cast<uint64_t>(end) > kMaxPlyFileBytes) {
    return StatusOr<std::vector<uint8_t>>::Error("file is too large");
  }
  if (static_cast<uint64_t>(end) > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
    return StatusOr<std::vector<uint8_t>>::Error("file is too large");
  }
  const size_t size = static_cast<size_t>(end);
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> out;
  try {
    out.resize(size);
  } catch (const std::bad_alloc&) {
    return StatusOr<std::vector<uint8_t>>::Error("file allocation failed");
  } catch (const std::length_error&) {
    return StatusOr<std::vector<uint8_t>>::Error("file is too large");
  }
  file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
  if (!file) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to read file");
  }
  return StatusOr<std::vector<uint8_t>>::Ok(std::move(out));
}

StatusOr<double> ParseAsciiScalarToken(const std::string& token) {
  size_t consumed = 0;
  try {
    const double value = std::stod(token, &consumed);
    if (consumed != token.size()) {
      return StatusOr<double>::Error("ascii ply parse error");
    }
    return StatusOr<double>::Ok(value);
  } catch (...) {
    return StatusOr<double>::Error("ascii ply parse error");
  }
}

StatusOr<double> ReadScalarAsDouble(const std::vector<uint8_t>& bytes, size_t& cursor, PlyScalarType type) {
  const size_t size = ScalarTypeSize(type);
  if (size == 0 || cursor > bytes.size() || size > bytes.size() - cursor) {
    return StatusOr<double>::Error("binary ply scalar read out of bounds");
  }

  const uint8_t* ptr = bytes.data() + cursor;
  cursor += size;
  switch (type) {
    case PlyScalarType::Int8:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<int8_t>(ptr)));
    case PlyScalarType::UInt8:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<uint8_t>(ptr)));
    case PlyScalarType::Int16:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<int16_t>(ptr)));
    case PlyScalarType::UInt16:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<uint16_t>(ptr)));
    case PlyScalarType::Int32:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<int32_t>(ptr)));
    case PlyScalarType::UInt32:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<uint32_t>(ptr)));
    case PlyScalarType::Float32:
      return StatusOr<double>::Ok(static_cast<double>(ReadLE<float>(ptr)));
    case PlyScalarType::Float64:
      return StatusOr<double>::Ok(ReadLE<double>(ptr));
    default:
      return StatusOr<double>::Error("unsupported scalar type");
  }
}

Status ValidatePlyLayout(const std::vector<uint8_t>& bytes, size_t bodyCursor, const PlyFile& ply) {
  if (bodyCursor > bytes.size()) {
    return Status::Error("invalid ply body");
  }
  if (ply.elements.size() > kMaxPlyElements) {
    return Status::Error("too many ply elements");
  }

  const uint64_t remainingBytes = static_cast<uint64_t>(bytes.size() - bodyCursor);
  uint64_t totalScalarBytes = 0;
  uint64_t minBinaryBytes = 0;
  uint64_t minAsciiBytes = 0;

  for (const PlyElement& elem : ply.elements) {
    if (elem.properties.size() > kMaxPlyProperties) {
      return Status::Error("too many ply properties");
    }

    uint64_t scalarProps = 0;
    uint64_t rowBinaryBytes = 0;
    uint64_t rowAsciiTokens = 0;
    for (const PlyProperty& prop : elem.properties) {
      if (prop.isList) {
        const size_t countBytes = ScalarTypeSize(prop.listCountType);
        const size_t valueBytes = ScalarTypeSize(prop.listValueType);
        if (countBytes == 0 || valueBytes == 0) {
          return Status::Error("unsupported scalar type");
        }
        if (rowBinaryBytes > std::numeric_limits<uint64_t>::max() - countBytes) {
          return Status::Error("ply row is too large");
        }
        rowBinaryBytes += countBytes;
      } else {
        const size_t valueBytes = ScalarTypeSize(prop.type);
        if (valueBytes == 0) {
          return Status::Error("unsupported scalar type");
        }
        if (rowBinaryBytes > std::numeric_limits<uint64_t>::max() - valueBytes) {
          return Status::Error("ply row is too large");
        }
        rowBinaryBytes += valueBytes;
        ++scalarProps;
      }
      ++rowAsciiTokens;
    }

    if (scalarProps > 0) {
      if (elem.count > std::numeric_limits<uint64_t>::max() / scalarProps) {
        return Status::Error("ply element/property data is too large for the configured loader limits");
      }
      const uint64_t scalarValues = static_cast<uint64_t>(elem.count) * scalarProps;
      if (scalarValues > std::numeric_limits<uint64_t>::max() / sizeof(double)) {
        return Status::Error("ply element/property data is too large for the configured loader limits");
      }
      const uint64_t scalarBytes = scalarValues * sizeof(double);
      if (scalarBytes > kMaxPlyScalarBytes - totalScalarBytes) {
        return Status::Error("ply element/property data is too large for the configured loader limits");
      }
      totalScalarBytes += scalarBytes;
    }

    if (elem.count != 0) {
      if (rowBinaryBytes != 0 && elem.count > std::numeric_limits<uint64_t>::max() / rowBinaryBytes) {
        return Status::Error("ply body is too large");
      }
      if (rowAsciiTokens != 0 && elem.count > std::numeric_limits<uint64_t>::max() / rowAsciiTokens) {
        return Status::Error("ply body is too large");
      }
      const uint64_t binaryBytes = static_cast<uint64_t>(elem.count) * rowBinaryBytes;
      const uint64_t asciiBytes = static_cast<uint64_t>(elem.count) * rowAsciiTokens;
      if (minBinaryBytes > std::numeric_limits<uint64_t>::max() - binaryBytes ||
          minAsciiBytes > std::numeric_limits<uint64_t>::max() - asciiBytes) {
        return Status::Error("ply body is too large");
      }
      minBinaryBytes += binaryBytes;
      minAsciiBytes += asciiBytes;
    }
  }

  if (ply.format == PlyFormat::BinaryLittleEndian && minBinaryBytes > remainingBytes) {
    return Status::Error("binary ply scalar read out of bounds");
  }
  if (ply.format == PlyFormat::Ascii && minAsciiBytes > remainingBytes) {
    return Status::Error("ascii ply parse error");
  }
  return Status::Ok();
}

StatusOr<PlyFile> ParsePlyBytes(const std::vector<uint8_t>& bytes) try {
  PlyFile ply{};

  size_t cursor = 0;
  bool headerLineTooLarge = false;
  auto readLine = [&](std::string& outLine) -> bool {
    if (cursor >= bytes.size()) {
      return false;
    }
    size_t end = cursor;
    while (end < bytes.size() && bytes[end] != '\n') {
      ++end;
    }
    if (end - cursor > kMaxPlyHeaderLineBytes) {
      headerLineTooLarge = true;
      return false;
    }
    outLine.assign(reinterpret_cast<const char*>(bytes.data() + cursor), end - cursor);
    if (!outLine.empty() && outLine.back() == '\r') {
      outLine.pop_back();
    }
    cursor = (end < bytes.size()) ? (end + 1) : end;
    return true;
  };

  std::string line;
  if (!readLine(line)) {
    if (headerLineTooLarge) {
      return StatusOr<PlyFile>::Error("ply header line too large");
    }
    return StatusOr<PlyFile>::Error("invalid ply magic");
  }
  if (Trim(line) != "ply") {
    return StatusOr<PlyFile>::Error("invalid ply magic");
  }

  PlyElement* currentElement = nullptr;
  bool formatSet = false;
  bool headerEnded = false;

  while (readLine(line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed == "end_header") {
      headerEnded = true;
      break;
    }

    const std::vector<std::string> tokens = SplitBySpaces(trimmed);
    if (tokens.empty()) {
      continue;
    }

    if (tokens[0] == "comment") {
      if (trimmed.size() > 8) {
        ply.comments.emplace_back(trimmed.substr(8));
      }
      continue;
    }

    if (tokens[0] == "format") {
      if (tokens.size() < 3) {
        return StatusOr<PlyFile>::Error("invalid format line");
      }
      const std::string f = ToLower(tokens[1]);
      if (f == "ascii") {
        ply.format = PlyFormat::Ascii;
      } else if (f == "binary_little_endian") {
        ply.format = PlyFormat::BinaryLittleEndian;
      } else {
        return StatusOr<PlyFile>::Error("unsupported ply format");
      }
      formatSet = true;
      continue;
    }

    if (tokens[0] == "element") {
      if (tokens.size() < 3) {
        return StatusOr<PlyFile>::Error("invalid element line");
      }
      PlyElement elem{};
      elem.name = tokens[1];
      const auto count = ParseUint32(tokens[2]);
      if (!count.ok()) {
        return StatusOr<PlyFile>::Error(count.status.message);
      }
      elem.count = count.value;
      if (ply.elements.size() >= kMaxPlyElements) {
        return StatusOr<PlyFile>::Error("too many ply elements");
      }
      ply.elements.push_back(std::move(elem));
      currentElement = &ply.elements.back();
      continue;
    }

    if (tokens[0] == "property") {
      if (currentElement == nullptr) {
        return StatusOr<PlyFile>::Error("property before element");
      }
      if (currentElement->properties.size() >= kMaxPlyProperties) {
        return StatusOr<PlyFile>::Error("too many ply properties");
      }
      PlyProperty prop{};
      if (tokens.size() >= 5 && tokens[1] == "list") {
        prop.isList = true;
        prop.listCountType = ParseScalarType(tokens[2]);
        prop.listValueType = ParseScalarType(tokens[3]);
        prop.name = tokens[4];
        if (ScalarTypeSize(prop.listCountType) == 0 || ScalarTypeSize(prop.listValueType) == 0) {
          return StatusOr<PlyFile>::Error("unsupported scalar type");
        }
        if (!IsIntegerScalarType(prop.listCountType)) {
          return StatusOr<PlyFile>::Error("unsupported ply list count type");
        }
      } else if (tokens.size() >= 3) {
        prop.type = ParseScalarType(tokens[1]);
        prop.name = tokens[2];
        if (ScalarTypeSize(prop.type) == 0) {
          return StatusOr<PlyFile>::Error("unsupported scalar type");
        }
      } else {
        return StatusOr<PlyFile>::Error("invalid property line");
      }
      currentElement->properties.push_back(std::move(prop));
      continue;
    }
  }

  if (headerLineTooLarge) {
    return StatusOr<PlyFile>::Error("ply header line too large");
  }
  if (!formatSet || !headerEnded) {
    return StatusOr<PlyFile>::Error("invalid ply header");
  }

  const Status layout = ValidatePlyLayout(bytes, cursor, ply);
  if (!layout.ok) {
    return StatusOr<PlyFile>::Error(layout.message);
  }

  try {
    for (auto& elem : ply.elements) {
      elem.scalarColumns.resize(elem.properties.size());
      for (size_t i = 0; i < elem.properties.size(); ++i) {
        if (!elem.properties[i].isList) {
          elem.scalarColumns[i].resize(elem.count);
        }
      }
    }
  } catch (const std::bad_alloc&) {
    return StatusOr<PlyFile>::Error("ply allocation failed");
  } catch (const std::length_error&) {
    return StatusOr<PlyFile>::Error("ply element/property data is too large for the configured loader limits");
  }

  if (ply.format == PlyFormat::Ascii) {
    size_t asciiCursor = cursor;
    auto readToken = [&]() -> StatusOr<std::string> {
      while (asciiCursor < bytes.size() && std::isspace(static_cast<unsigned char>(bytes[asciiCursor]))) {
        ++asciiCursor;
      }
      if (asciiCursor >= bytes.size()) {
        return StatusOr<std::string>::Error("ascii ply parse error");
      }
      const size_t tokenBegin = asciiCursor;
      while (asciiCursor < bytes.size() && !std::isspace(static_cast<unsigned char>(bytes[asciiCursor]))) {
        ++asciiCursor;
      }
      if (asciiCursor - tokenBegin > kMaxAsciiPlyTokenBytes) {
        return StatusOr<std::string>::Error("ascii ply token too large");
      }
      return StatusOr<std::string>::Ok(
          std::string(reinterpret_cast<const char*>(bytes.data() + tokenBegin), asciiCursor - tokenBegin));
    };

    for (auto& elem : ply.elements) {
      for (uint32_t row = 0; row < elem.count; ++row) {
        for (size_t pi = 0; pi < elem.properties.size(); ++pi) {
          const PlyProperty& prop = elem.properties[pi];
          auto token = readToken();
          if (!token.ok()) {
            return StatusOr<PlyFile>::Error(token.status.message);
          }

          if (prop.isList) {
            const auto count = ParseAsciiScalarToken(token.value);
            if (!count.ok()) {
              return StatusOr<PlyFile>::Error(count.status.message);
            }
            if (!std::isfinite(count.value) || count.value < 0.0 ||
                count.value > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
              return StatusOr<PlyFile>::Error("ascii ply list count out of range");
            }
            if (std::floor(count.value) != count.value) {
              return StatusOr<PlyFile>::Error("ascii ply list count out of range");
            }
            const uint32_t listCount = static_cast<uint32_t>(std::max(0.0, count.value));
            for (uint32_t i = 0; i < listCount; ++i) {
              token = readToken();
              if (!token.ok()) {
                return StatusOr<PlyFile>::Error(token.status.message);
              }
            }
          } else {
            const auto value = ParseAsciiScalarToken(token.value);
            if (!value.ok()) {
              return StatusOr<PlyFile>::Error(value.status.message);
            }
            elem.scalarColumns[pi][row] = value.value;
          }
        }
      }
    }
  } else {
    for (auto& elem : ply.elements) {
      for (uint32_t row = 0; row < elem.count; ++row) {
        for (size_t pi = 0; pi < elem.properties.size(); ++pi) {
          const PlyProperty& prop = elem.properties[pi];
          if (prop.isList) {
            const auto countValue = ReadScalarAsDouble(bytes, cursor, prop.listCountType);
            if (!countValue.ok()) {
              return StatusOr<PlyFile>::Error(countValue.status.message);
            }
            if (!std::isfinite(countValue.value) || countValue.value < 0.0 ||
                countValue.value > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
              return StatusOr<PlyFile>::Error("binary ply list count out of range");
            }
            if (std::floor(countValue.value) != countValue.value) {
              return StatusOr<PlyFile>::Error("binary ply list count out of range");
            }
            const uint32_t listCount = static_cast<uint32_t>(std::max(0.0, countValue.value));
            for (uint32_t i = 0; i < listCount; ++i) {
              const auto ignored = ReadScalarAsDouble(bytes, cursor, prop.listValueType);
              if (!ignored.ok()) {
                return StatusOr<PlyFile>::Error(ignored.status.message);
              }
            }
          } else {
            const auto value = ReadScalarAsDouble(bytes, cursor, prop.type);
            if (!value.ok()) {
              return StatusOr<PlyFile>::Error(value.status.message);
            }
            elem.scalarColumns[pi][row] = value.value;
          }
        }
      }
    }
  }

  return StatusOr<PlyFile>::Ok(std::move(ply));
} catch (const std::bad_alloc&) {
  return StatusOr<PlyFile>::Error("ply allocation failed");
} catch (const std::length_error&) {
  return StatusOr<PlyFile>::Error("ply allocation failed");
}

}  // namespace

size_t ScalarTypeSize(PlyScalarType type) {
  switch (type) {
    case PlyScalarType::Int8:
    case PlyScalarType::UInt8:
      return 1;
    case PlyScalarType::Int16:
    case PlyScalarType::UInt16:
      return 2;
    case PlyScalarType::Int32:
    case PlyScalarType::UInt32:
    case PlyScalarType::Float32:
      return 4;
    case PlyScalarType::Float64:
      return 8;
    default:
      return 0;
  }
}

PlyScalarType ParseScalarType(const std::string& token) {
  const std::string t = ToLower(token);
  if (t == "char" || t == "int8") return PlyScalarType::Int8;
  if (t == "uchar" || t == "uint8") return PlyScalarType::UInt8;
  if (t == "short" || t == "int16") return PlyScalarType::Int16;
  if (t == "ushort" || t == "uint16") return PlyScalarType::UInt16;
  if (t == "int" || t == "int32") return PlyScalarType::Int32;
  if (t == "uint" || t == "uint32") return PlyScalarType::UInt32;
  if (t == "float" || t == "float32") return PlyScalarType::Float32;
  if (t == "double" || t == "float64") return PlyScalarType::Float64;
  return PlyScalarType::Unknown;
}

StatusOr<PlyFile> ReadPlyFile(const std::string& path) {
  const auto bytes = ReadWholeFile(path);
  if (!bytes.ok()) {
    return StatusOr<PlyFile>::Error(bytes.status.message);
  }
  return ParsePlyBytes(bytes.value);
}

const PlyElement* FindElement(const PlyFile& file, const std::string& name) {
  for (const auto& element : file.elements) {
    if (element.name == name) {
      return &element;
    }
  }
  return nullptr;
}

int FindScalarProperty(const PlyElement& element, const std::string& name) {
  for (size_t i = 0; i < element.properties.size(); ++i) {
    if (!element.properties[i].isList && element.properties[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}

