#pragma once
#include "runtime/JsonValue.hpp"
#include "connectors/Abort.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace batbox::mcp {class StdioClient;}
namespace batbox {
// Scrapling 0.3 is the first release with an MCP server, and its `get` tool
// already returns the {status, content, url} page Batbox reads.
inline constexpr const char* minimumScraplingVersion="0.3";
// Tools that do a plain HTTP fetch, in preference order: `make_request` from
// Scrapling 0.4.15 on, `get` from 0.3 to 0.4.14. Both take `url` and
// `extraction_type` and return the same page.
inline const std::vector<std::string>& scraplingPlainFetchTools(){
  static const std::vector<std::string> names{"make_request","get"};
  return names;
}
// The running Scrapling advertises no tool Batbox can use for a request.
struct ScraplingToolMissing:std::runtime_error {
  std::vector<std::string> wanted,offered;
  ScraplingToolMissing(std::vector<std::string> wantedTools,std::vector<std::string> offeredTools,
    const std::optional<std::string>& version);
};
// withFix ends the message with "Run: batbox setup web".
std::string scraplingToolMissingMessage(const std::vector<std::string>& wanted,
  const std::vector<std::string>& offered,const std::optional<std::string>& version,
  bool withFix=true);
// Tool names and input schemas from one tools/list result.
struct ScraplingTool {std::string name;Json inputSchema;};
std::vector<ScraplingTool> scraplingTools(const Json& list);
// The first wanted tool the server offers, or nullptr.
const ScraplingTool* chooseScraplingTool(const std::vector<ScraplingTool>& offered,
  const std::vector<std::string>& wanted);

// The built-in Scrapling connection: Batbox's MCP stdio client running
// `scrapling mcp`. The process starts on the first request, so a missing
// helper never affects startup, and restarts on a later request if it exited.
// Keeping it alive preserves the server's browser sessions between requests.
// Tool names come from the server's tools/list, read once per process.
class Scrapling {
  std::vector<std::string> command;
  std::chrono::milliseconds timeout;
  std::function<std::optional<std::string>()> packageVersion;
  mutable std::mutex state;
  std::timed_mutex starting;
  std::shared_ptr<mcp::StdioClient> client;
  std::optional<std::vector<ScraplingTool>> tools;
  std::atomic<bool> stopped{false};
  std::shared_ptr<mcp::StdioClient> live()const;
  std::shared_ptr<mcp::StdioClient> connection(std::stop_token signal);
public:
  static constexpr std::chrono::milliseconds defaultTimeout=std::chrono::hours(2);
  // packageVersion names the installed Scrapling in tool mismatch errors.
  explicit Scrapling(const std::vector<std::string>& command,
    std::chrono::milliseconds timeout=defaultTimeout,
    std::function<std::optional<std::string>()> packageVersion={});
  ~Scrapling();
  // After shutdown no request starts the helper again.
  void shutdown()noexcept;
  // True while a helper process is running.
  bool running()const noexcept;
  std::string stderrDiagnostic()const;
  Scrapling(const Scrapling&) = delete;
  Scrapling& operator=(const Scrapling&) = delete;
  Json listGadgets(std::stop_token signal={});
  // The raw tools/call result, including isError results.
  Json callGadget(const std::string& name, const Json& arguments,std::stop_token signal={});
  // The tools the running server advertises, listed once per process.
  std::vector<ScraplingTool> advertisedGadgets(std::stop_token signal={});
  // The advertised tool for a plain fetch; throws ScraplingToolMissing.
  ScraplingTool fetchGadget(std::stop_token signal={});
  Json fetch(const std::string& url,std::stop_token signal={});
};
} // namespace batbox
