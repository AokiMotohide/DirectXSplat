#pragma once

#include <bit>
#include <cstdint>

namespace directxsplat {

enum class SortBackend {
  OneSweep,
  Custom,
};

inline uint32_t EncodeDepthSortKeyBackToFront(float depth) {
  const uint32_t bits = std::bit_cast<uint32_t>(depth);
  const uint32_t mask = (bits & 0x80000000u) ? 0xFFFFFFFFu : 0x80000000u;
  const uint32_t sortableAscending = bits ^ mask;
  return ~sortableAscending;
}

}  // namespace directxsplat
