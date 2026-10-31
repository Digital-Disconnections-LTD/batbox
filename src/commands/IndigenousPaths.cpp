#include "commands/IndigenousPaths.hpp"
#include "runtime/Encoding.hpp"
#include <filesystem>

namespace batbox::commands {
event_loop::Deferred<bool> indigenousPathExists(event_loop::Loop& loop,
  const std::string& path){
  event_loop::Deferred<bool> output(loop);
  const auto encoded=value::encodeUtf8(path);
  loop.queueMicrotask([output,encoded]{
    std::error_code error;
    const auto status=std::filesystem::status(encoded,error);
    output.resolve(!error&&std::filesystem::exists(status));
  });
  return output;
}
} // namespace batbox::commands
