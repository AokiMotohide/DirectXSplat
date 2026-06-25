#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());

  std::ostringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

std::string TrimAsciiWhitespace(std::string text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }

  const size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

std::vector<std::filesystem::path> FindExampleSources(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> sources;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const std::filesystem::path extension = entry.path().extension();
    if (extension == ".cpp" || extension == ".h" || extension == ".hpp") {
      sources.push_back(entry.path());
    }
  }

  std::ranges::sort(sources);
  return sources;
}

}

TEST_CASE("Examples include only the public header") {
  const std::filesystem::path examplesRoot = DIRECTXSPLAT_EXAMPLES_DIR;
  REQUIRE(std::filesystem::is_directory(examplesRoot));

  const std::vector<std::filesystem::path> sources = FindExampleSources(examplesRoot);
  CHECK(sources.size() >= 3);

  const std::vector<std::string> forbiddenText = {
      "d3d12.h",
      "dxgi.h",
      "dxsplat/renderer.h",
      "dxsplat/context.h",
      "dxsplat/gpu_resources.h",
      "dxsplat/io.h",
  };

  for (const std::filesystem::path& path : sources) {
    CAPTURE(path.string());
    const std::string text = ReadText(path);

    for (const std::string& forbidden : forbiddenText) {
      CHECK(text.find(forbidden) == std::string::npos);
    }

    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
      const std::string include = TrimAsciiWhitespace(line);
      if (!include.starts_with("#include")) {
        continue;
      }

      CHECK(include.find("#include \"") == std::string::npos);
      if (include.find("#include <dxsplat/") != std::string::npos) {
        CHECK(include == "#include <dxsplat/directxsplat.h>");
      } else if (include.starts_with("#include <")) {
        CHECK(include.find('/') == std::string::npos);
        CHECK_FALSE(include.ends_with(".h>"));
      }
    }
  }
}
