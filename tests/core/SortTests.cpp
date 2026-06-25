#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "dxsplat/math.h"
#include "dxsplat/sort.h"

namespace directxsplat {
namespace {

std::vector<size_t> SortIndicesBackToFront(const std::vector<Vec3>& positions, const Mat4& view) {
  struct SortItem {
    uint32_t key = 0;
    size_t index = 0;
  };

  std::vector<SortItem> items;
  items.reserve(positions.size());
  for (size_t i = 0; i < positions.size(); ++i) {
    const Vec4 viewPosition = Mul(view, Vec4{positions[i].x, positions[i].y, positions[i].z, 1.0f});
    items.push_back({EncodeDepthSortKeyBackToFront(viewPosition.z), i});
  }
  std::stable_sort(items.begin(), items.end(), [](const SortItem& a, const SortItem& b) {
    return a.key < b.key;
  });

  std::vector<size_t> sorted;
  sorted.reserve(items.size());
  for (const SortItem& item : items) {
    sorted.push_back(item.index);
  }
  return sorted;
}

}  

TEST_CASE("Sort handles empty input") {
  const Mat4 view = LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
  CHECK(SortIndicesBackToFront({}, view).empty());
}

TEST_CASE("Sort handles a single splat") {
  const Mat4 view = LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
  const auto sorted = SortIndicesBackToFront({Vec3{0.0f, 0.0f, 2.0f}}, view);
  REQUIRE(sorted.size() == 1u);
  CHECK(sorted[0] == 0u);
}

TEST_CASE("Sort orders a small synthetic set back to front") {
  const Mat4 view = LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
  const std::vector<Vec3> positions{{0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 4.0f}};
  const auto sorted = SortIndicesBackToFront(positions, view);
  REQUIRE(sorted.size() == 3u);
  CHECK(sorted[0] == 1u);
  CHECK(sorted[1] == 2u);
  CHECK(sorted[2] == 0u);
}

TEST_CASE("Sort ordering changes when the camera moves") {
  const std::vector<Vec3> positions{{0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 4.0f}};
  const Mat4 forwardView = LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
  const Mat4 reverseView = LookAt({0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 5.0f}, {0.0f, 1.0f, 0.0f});

  const auto forwardSorted = SortIndicesBackToFront(positions, forwardView);
  const auto reverseSorted = SortIndicesBackToFront(positions, reverseView);

  REQUIRE(forwardSorted.size() == 2u);
  REQUIRE(reverseSorted.size() == 2u);
  CHECK(forwardSorted[0] == 1u);
  CHECK(forwardSorted[1] == 0u);
  CHECK(reverseSorted[0] == 0u);
  CHECK(reverseSorted[1] == 1u);
}

}  // namespace directxsplat
