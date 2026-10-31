#pragma once
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace batbox::file_state_stash {
inline constexpr std::size_t READ_FILE_STATE_STASH_SIZE=100;
inline constexpr std::size_t DEFAULT_MAX_STASH_SIZE_BYTES=25*1024*1024;

struct FileState {
  std::string content;
  double timestamp=0;
  bool isPartialView=false;
  std::optional<double> offset,limit;
  bool partialViewPresent=false;
  FileState()=default;
  FileState(std::string content,double timestamp,bool isPartialView=false,
    std::optional<double> offset={},std::optional<double> limit={},
    bool partialViewPresent=false):content(std::move(content)),
    timestamp(timestamp),isPartialView(isPartialView),offset(offset),
    limit(limit),partialViewPresent(partialViewPresent){}
};
Json toJson(const FileState& value);
FileState fromJson(const Json& value);
std::string normalizeKey(const std::string& path);

class FileStateStash {
 public:
  using Value=std::shared_ptr<FileState>;
  struct DumpEntry {std::string key;Value value;std::size_t size;};
  using Dump=std::vector<DumpEntry>;
  using Entries=std::vector<std::pair<std::string,Value>>;
  explicit FileStateStash(std::size_t maxEntries,
    std::size_t maxSizeBytes=DEFAULT_MAX_STASH_SIZE_BYTES);
  FileStateStash(const FileStateStash&)=delete;
  FileStateStash& operator=(const FileStateStash&)=delete;

  Value get(const std::string& key);
  bool has(const std::string& key)const;
  FileStateStash& set(const std::string& key,FileState value);
  FileStateStash& set(const std::string& key,Value value);
  bool remove(const std::string& key);
  void clear();
  std::size_t size()const{return index_.size();}
  std::size_t max()const{return maxEntries_;}
  std::size_t maxSize()const{return maxSizeBytes_;}
  std::size_t calculatedSize()const{return calculatedSize_;}
  std::vector<std::string> keys()const;
  Entries entries()const;
  Dump dump()const;
  void load(const Dump& entries);
  Json dumpJson()const;
  void loadJson(const Json& entries);
 private:
  struct Entry {std::string key;Value value;std::size_t size;};
  std::size_t maxEntries_,maxSizeBytes_,calculatedSize_=0;
  std::list<Entry> order_; // most recently used first
  std::unordered_map<std::string,std::list<Entry>::iterator> index_;
  static std::size_t valueSize(const FileState& value);
  FileStateStash& setSized(std::string key,Value value,std::size_t size);
  void erase(std::list<Entry>::iterator entry);
  void evict();
};

std::shared_ptr<FileStateStash> createFileStateStashWithSizeLimit(
  std::size_t maxEntries,std::size_t maxSizeBytes=DEFAULT_MAX_STASH_SIZE_BYTES);
FileStateStash::Entries stashToObject(const FileStateStash& stash);
Json stashToJson(const FileStateStash& stash);
std::vector<std::string> stashKeys(const FileStateStash& stash);
std::shared_ptr<FileStateStash> cloneFileStateStash(const FileStateStash& stash);
std::shared_ptr<FileStateStash> mergeFileStateStashes(
  const FileStateStash& first,const FileStateStash& second);
} // namespace batbox::file_state_stash
