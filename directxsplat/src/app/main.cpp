#include <Windows.h>

#include <shellapi.h>

#include <string>
#include <vector>

#include "app/Application.h"
#include "tools/CliOptions.h"

namespace {

std::vector<std::string> GetUtf8Args() {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> args;
  args.reserve(argc > 0 ? static_cast<size_t>(argc - 1) : 0);

  for (int i = 1; i < argc; ++i) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
    std::string arg(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
    WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg.data(), bytes, nullptr, nullptr);
    args.push_back(std::move(arg));
  }

  LocalFree(argv);
  return args;
}

}  

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  using namespace dxsplat;

  const auto parse = ParseCliOptions(GetUtf8Args());
  if (!parse.ok()) {
    MessageBoxA(nullptr, parse.status.message.c_str(), "DirectXSplat", MB_OK | MB_ICONERROR);
    return 1;
  }

  if (parse.value.showHelp) {
    MessageBoxA(nullptr,
                "DirectXSplatImGuiViewer [scene_path]\n"
                "  --scene-folder <folder>\n"
                "  --render-size <W>x<H>\n"
                "  --images-path <dir>",
                "DirectXSplat", MB_OK);
    return 0;
  }

  ViewerConfig config{};
  config.initialScenePath = parse.value.scenePath.value_or("");
  config.sceneFolderPath = parse.value.folderTraversalPath.value_or("");
  config.sourceImageDirectory = parse.value.imagePathOverride.value_or("");
  config.width = parse.value.renderWidthOverride.value_or(1600);
  config.height = parse.value.renderHeightOverride.value_or(900);

  Application app;
  const Status init = app.Initialize(config);
  if (!init.ok) {
    MessageBoxA(nullptr, init.message.c_str(), "DirectXSplat", MB_OK | MB_ICONERROR);
    return 1;
  }

  const Status run = app.Run();
  if (!run.ok) {
    MessageBoxA(nullptr, run.message.c_str(), "DirectXSplat", MB_OK | MB_ICONERROR);
    return 1;
  }
  return 0;
}
