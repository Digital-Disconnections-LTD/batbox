#pragma once
#include "diagnostics/ScopeAdapter.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::fs_operations {
struct ReadSyncResult {
  std::vector<unsigned char> buffer;
  std::size_t bytesRead=0;
};
struct NodeFsSyncImports {
  diagnostics::BeginSlowLogging beginSlowLogging;
  std::function<std::string()> cwd;
  std::function<bool(const std::string&)> existsSync;
  std::function<Json(const std::string&)> statSync,lstatSync;
  std::function<std::string(const std::string&,const std::string&)> readFileSync;
  std::function<std::vector<unsigned char>(const std::string&)> readFileBytesSync;
  std::function<int(const std::string&,const std::string&,
    const std::optional<Json>&)> openSync;
  std::function<std::size_t(int,std::vector<unsigned char>&,std::size_t,
    std::size_t,std::size_t)> readSync;
  std::function<void(int)> closeSync;
  std::function<void(int,const std::string&)> appendFdSync;
  std::function<void(const std::string&,const std::string&)> appendPathSync;
  std::function<std::optional<std::string>(const std::exception_ptr&)> errnoCode;
  std::function<void(const std::string&,const std::string&)> copyFileSync,
    renameSync,linkSync;
  std::function<void(const std::string&)> unlinkSync,rmdirSync;
  std::function<void(const std::string&,const std::string&,
    const std::optional<std::string>&)> symlinkSync;
  std::function<std::string(const std::string&)> readlinkSync,realpathSync;
  std::function<void(const std::string&,const Json&)> mkdirSync;
  std::function<void(const std::string&,const std::optional<Json>&)> rmSync;
  std::function<Json(const std::string&,bool)> readdirSync;
  std::function<std::vector<std::string>(const std::string&)> readdirStringSync;
};

class NodeFsSync {
public:
  explicit NodeFsSync(NodeFsSyncImports imports):imports_(std::move(imports)){}
  std::string cwd()const;
  bool existsSync(const std::string& path)const;
  Json statSync(const std::string& path)const;
  Json lstatSync(const std::string& path)const;
  std::string readFileSync(const std::string& path,const std::string& encoding)const;
  std::vector<unsigned char> readFileBytesSync(const std::string& path)const;
  ReadSyncResult readSync(const std::string& path,std::size_t length)const;
  void appendFileSync(const std::string& path,const std::string& data,
    std::optional<Json> options={})const;
  void copyFileSync(const std::string& source,const std::string& destination)const;
  void unlinkSync(const std::string& path)const;
  void renameSync(const std::string& source,const std::string& destination)const;
  void linkSync(const std::string& target,const std::string& path)const;
  void symlinkSync(const std::string& target,const std::string& path,
    std::optional<std::string> type={})const;
  std::string readlinkSync(const std::string& path)const;
  std::string realpathSync(const std::string& path)const;
  void mkdirSync(const std::string& path,std::optional<Json> options={})const;
  Json readdirSync(const std::string& path)const;
  std::vector<std::string> readdirStringSync(const std::string& path)const;
  bool isDirEmptySync(const std::string& path)const;
  void rmdirSync(const std::string& path)const;
  void rmSync(const std::string& path,std::optional<Json> options={})const;
private:
  NodeFsSyncImports imports_;
};
}
