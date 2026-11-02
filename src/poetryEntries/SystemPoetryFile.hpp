#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
// system.md in the configuration directory replaces the built-in static head
// when it holds text beyond comments and whitespace.
struct SystemPoetrySource {
  // Where the override is looked for; empty without a configuration directory.
  std::filesystem::path path;
  bool fromFile=false;
  // Prepared head in use, placeholders unrendered.
  std::string head;
  // Size of the file in use, or of the built-in source.
  std::uintmax_t bytes=0;
  std::size_t placeholders=0;
  std::vector<std::string> unknown;
  // Why an existing file is not used; empty otherwise.
  std::string problem;
};
std::filesystem::path systemPoetryPath(const std::filesystem::path& configDirectory);
SystemPoetrySource loadSystemPoetry(const std::filesystem::path& configDirectory);
// "built-in" or "~/.batbox/system.md · N bytes · M placeholders".
std::string describeSystemPoetry(const SystemPoetrySource& source);
// An unusable file or unknown placeholders; empty when nothing needs attention.
std::vector<std::string> systemPoetryWarnings(const SystemPoetrySource& source);
// The path with the home directory shown as ~.
std::string displayPath(const std::filesystem::path& path);

// Reloads system.md when its size or modification time changes, the same
// check BATBOX.md gets, so an edit applies from the next request.
class SystemPoetryFile {
public:
  explicit SystemPoetryFile(std::filesystem::path configDirectory);
  // True on the first call and whenever the file changed since the last one.
  bool refresh();
  const SystemPoetrySource& source()const{return source_;}
private:
  std::filesystem::path configDirectory_;
  std::string stamp_;
  bool loaded_=false;
  SystemPoetrySource source_;
};
}
