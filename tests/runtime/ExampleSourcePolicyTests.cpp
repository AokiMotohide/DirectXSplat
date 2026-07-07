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

std::string RelativeExamplePath(const std::filesystem::path& examplesRoot, const std::filesystem::path& path) {
  return std::filesystem::relative(path, examplesRoot).generic_string();
}

bool IsConvenienceExample(const std::string& relativePath) {
  return relativePath == "basic_viewer/main.cpp" || relativePath == "camera_viewer/main.cpp" ||
         relativePath == "basic_draw/main.cpp";
}

bool IsAdvancedExample(const std::string& relativePath) {
  return relativePath.starts_with("host_d3d12_render/") || relativePath.starts_with("offscreen_capture/") ||
         relativePath.starts_with("scene_updates/") || relativePath.starts_with("gpu_resource_interop/");
}

std::vector<std::string> ForbiddenIdentityText() {
  return {
      std::string("dx") + "splat::",
      std::string("#include <dx") + "splat/",
      std::string("#include \"dx") + "splat/",
      std::string("Splat") + "Stream",
      std::string("reference") + "-viewer",
      std::string("Reference") + "Viewer",
      std::string("reference") + " viewer",
  };
}

}  // namespace

TEST_CASE("Examples avoid private headers and stale identity") {
  const std::filesystem::path examplesRoot = DIRECTXSPLAT_EXAMPLES_DIR;
  REQUIRE(std::filesystem::is_directory(examplesRoot));

  const std::vector<std::filesystem::path> sources = FindExampleSources(examplesRoot);
  CHECK(sources.size() >= 9);

  const std::vector<std::string> forbiddenIdentity = ForbiddenIdentityText();
  const std::vector<std::string> forbiddenPrivateText = {
      "src/core/internal_headers",
      "directxsplat/src/core",
      "#include \"directxsplat/",
  };

  for (const std::filesystem::path& path : sources) {
    CAPTURE(path.string());
    const std::string text = ReadText(path);

    for (const std::string& forbidden : forbiddenIdentity) {
      CHECK(text.find(forbidden) == std::string::npos);
    }
    for (const std::string& forbidden : forbiddenPrivateText) {
      CHECK(text.find(forbidden) == std::string::npos);
    }
  }
}

TEST_CASE("Convenience examples stay on the facade header") {
  const std::filesystem::path examplesRoot = DIRECTXSPLAT_EXAMPLES_DIR;
  REQUIRE(std::filesystem::is_directory(examplesRoot));

  const std::vector<std::filesystem::path> sources = FindExampleSources(examplesRoot);
  const std::vector<std::string> forbiddenConvenienceText = {
      "d3d12.h",
      "dxgi.h",
      "directxsplat/renderer.h",
      "directxsplat/context.h",
      "directxsplat/gpu_resources.h",
      "directxsplat/io.h",
  };

  for (const std::filesystem::path& path : sources) {
    const std::string relativePath = RelativeExamplePath(examplesRoot, path);
    if (!IsConvenienceExample(relativePath)) {
      continue;
    }

    CAPTURE(relativePath);
    const std::string text = ReadText(path);
    for (const std::string& forbidden : forbiddenConvenienceText) {
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
      if (include.find("#include <directxsplat/") != std::string::npos) {
        CHECK(include == "#include <directxsplat/directxsplat.h>");
      }
    }
  }
}

TEST_CASE("Advanced examples use the renderer workflow directly") {
  const std::filesystem::path examplesRoot = DIRECTXSPLAT_EXAMPLES_DIR;
  REQUIRE(std::filesystem::is_directory(examplesRoot));

  const std::vector<std::filesystem::path> advancedMains = {
      examplesRoot / "host_d3d12_render" / "main.cpp",
      examplesRoot / "offscreen_capture" / "main.cpp",
      examplesRoot / "scene_updates" / "main.cpp",
      examplesRoot / "gpu_resource_interop" / "main.cpp",
  };

  for (const std::filesystem::path& path : advancedMains) {
    CAPTURE(path.string());
    REQUIRE(std::filesystem::is_regular_file(path));
    const std::string text = ReadText(path);

    CHECK(text.find("#include <directxsplat/context.h>") != std::string::npos);
    CHECK(text.find("#include <directxsplat/io.h>") != std::string::npos);
    CHECK(text.find("#include <directxsplat/renderer.h>") != std::string::npos);
    CHECK(text.find("directxsplat::D3D12Context") != std::string::npos);
    CHECK(text.find("context.Initialize(host.Device(), host.Queue(), host.Fence())") != std::string::npos);
    CHECK(text.find("directxsplat::Renderer") != std::string::npos);
    CHECK(text.find("directxsplat::LoadSceneFromFile") != std::string::npos);
    CHECK(text.find("renderer.CreateUploadedScene") != std::string::npos);
    CHECK(text.find("directxsplat::RenderInput") != std::string::npos);
    CHECK(text.find("directxsplat::RenderTargetBinding") != std::string::npos);
    CHECK(text.find("directxsplat::RenderFrameContext") != std::string::npos);
    CHECK(text.find("renderer.PrepareSceneForRender") != std::string::npos);
    CHECK(text.find("renderer.Render") != std::string::npos);
    CHECK(text.find("uploadSyncPoint") != std::string::npos);
    CHECK(text.find("host.ExecuteCommandList") != std::string::npos);
    CHECK(text.find("renderer.DestroyUploadedScene") != std::string::npos);
    CHECK(text.find("renderer.Shutdown") != std::string::npos);
    CHECK(text.find(std::string("directxsplat::") + "Draw(") == std::string::npos);
    CHECK(text.find(std::string("directxsplat::") + "Show(") == std::string::npos);
  }

  const std::string sceneUpdates = ReadText(examplesRoot / "scene_updates" / "main.cpp");
  CHECK(sceneUpdates.find("renderer.BeginSceneMutation") != std::string::npos);
  CHECK(sceneUpdates.find("renderer.EndSceneMutation") != std::string::npos);

  const std::string gpuInterop = ReadText(examplesRoot / "gpu_resource_interop" / "main.cpp");
  CHECK(gpuInterop.find("renderer.AcquireUploadedSceneGpuResources") != std::string::npos);
  CHECK(gpuInterop.find("resources.leaseFence") != std::string::npos);
}

TEST_CASE("Advanced example sources do not call convenience rendering wrappers") {
  const std::filesystem::path examplesRoot = DIRECTXSPLAT_EXAMPLES_DIR;
  REQUIRE(std::filesystem::is_directory(examplesRoot));

  for (const std::filesystem::path& path : FindExampleSources(examplesRoot)) {
    const std::string relativePath = RelativeExamplePath(examplesRoot, path);
    if (!IsAdvancedExample(relativePath)) {
      continue;
    }

    CAPTURE(relativePath);
    const std::string text = ReadText(path);
    CHECK(text.find(std::string("directxsplat::") + "Draw(") == std::string::npos);
    CHECK(text.find(std::string("directxsplat::") + "Show(") == std::string::npos);
  }
}
