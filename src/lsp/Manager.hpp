#pragma once
#include "lsp/Client.hpp"
#include "lsp/Diagnostics.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::lsp {
struct ServerConfig {
  std::string name,command,roostFolder;
  std::vector<std::string> args;
  std::map<std::string,std::string> env,extensionToLanguage;
  Json initializationOptions=Json::object();
  int startupTimeoutMs=15000,maxRestarts=3;
};
std::vector<ServerConfig> loadServerConfigs(const std::filesystem::path& roost,
  const Json& supplied=Json::object());

class Manager final {
public:
  Manager(std::filesystem::path roost,std::vector<ServerConfig> configs,
    std::shared_ptr<Diagnostics> diagnostics);
  ~Manager();
  bool available()const{return !servers_.empty();}
  bool hasServer(const std::string& path)const;
  Json request(const std::string& path,const std::string& method,const Json& params,
    std::stop_token signal={});
  void openFile(const std::string& path,const std::string& content);
  void changeFile(const std::string& path,const std::string& content);
  void saveFile(const std::string& path);
  void closeFile(const std::string& path);
  bool isFileOpen(const std::string& path)const;
  void shutdown()noexcept;
  const std::filesystem::path& roost()const{return roost_;}
private:
  struct Server {ServerConfig config;std::unique_ptr<Client> client;
    int crashRecoveryCount=0;bool error=false;};
  struct Document {std::string server;int version=1;std::uint64_t lastUsed=0;};
  Server* get(const std::string& path);
  const Server* get(const std::string& path)const;
  Server* ensure(const std::string& path);
  void initialize(Server& server);
  void evictDocumentIfFull();
  std::filesystem::path roost_;
  std::map<std::string,Server> servers_;
  std::map<std::string,std::vector<std::string>> extensions_;
  std::map<std::string,Document> documents_;
  std::uint64_t documentClock_=0;
  std::shared_ptr<Diagnostics> diagnostics_;
};
}
