#pragma once
#include <filesystem>
#include <memory>
#include <string>

namespace batbox::application_editor {
// Private append-only temporary transcript, deliberately retained for the
// external viewer launched by the `v` command.
class TranscriptFile final {
public:
  static std::shared_ptr<TranscriptFile> create();
  ~TranscriptFile();
  TranscriptFile(const TranscriptFile&)=delete;
  TranscriptFile& operator=(const TranscriptFile&)=delete;
  void append(const std::string& text);
  void finish();
  const std::filesystem::path& path()const{return path_;}
private:
  TranscriptFile(std::filesystem::path path,int fd):path_(std::move(path)),fd_(fd){}
  std::filesystem::path path_;
  int fd_=-1;
};
}
