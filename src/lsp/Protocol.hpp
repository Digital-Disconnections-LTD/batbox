#pragma once
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace batbox::lsp {
// LSP uses UTF-8 byte counts for the Content-Length transport header.
class FrameDecoder {
public:
  explicit FrameDecoder(std::function<void(Json)> receive):receive_(std::move(receive)){}
  void feed(std::string_view bytes);
private:
  std::function<void(Json)> receive_;
  std::string buffer_;
  std::size_t contentLength_=0;
  bool awaitingBody_=false;
};
std::string frame(const Json& message);
std::string fileUri(const std::filesystem::path& path);
std::filesystem::path pathFromFileUri(std::string_view uri);
std::string displayUri(std::string_view uri,const std::filesystem::path& cwd);
}
