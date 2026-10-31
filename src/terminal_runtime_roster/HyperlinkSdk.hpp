#pragma once
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::terminal_runtime_roster {
using Environment=std::unordered_map<std::string,std::string>;
struct DetectionContext {
  Environment env;
  std::vector<std::string> argv;
  bool stdoutIsTTY=false;
  std::string platform="linux";
  std::string osRelease;
};

// Indigenous selected-pin ports of supports-color 10.2.2 and
// supports-hyperlinks 4.5.0, restricted to the stdout call site.
bool hasFlag(const std::string& flag,
  const std::vector<std::string>& argv);
bool createSupportsColor(const DetectionContext& context);
bool createSupportsHyperlinks(const DetectionContext& context);
DetectionContext captureIndigenousDetectionContext(
  std::vector<std::string> processArgv);

struct SupportsHyperlinksOptions {
  std::optional<Environment> env;
  std::optional<bool> stdoutSupported;
};
class HyperlinkSupport {
public:
  HyperlinkSupport(DetectionContext context,
    std::function<Environment()> currentProcessEnv);
  bool supportsHyperlinks(
    const SupportsHyperlinksOptions& options={})const;
private:
  std::function<Environment()> currentProcessEnv_;
  bool libraryStdout_;
};
Environment captureIndigenousEnvironment();
HyperlinkSupport makeIndigenousHyperlinkSupport(
  std::vector<std::string> processArgv);
}
