#include "lsp/Manager.hpp"
#include "lsp/Protocol.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <uv.h>

namespace batbox::lsp {
namespace {
std::string lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),
  [](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
Json readConfig(const std::filesystem::path& path){
  std::ifstream in(path);if(!in)return Json::object();
  try{return Json::parse(in);}catch(...){return Json::object();}
}
Json serversFrom(const Json& input){
  if(!input.is_object())return Json::object();
  if(input.contains("servers")&&input.at("servers").is_object())return input.at("servers");
  if(input.contains("lspServers")&&input.at("lspServers").is_object())return input.at("lspServers");
  // A direct server map is useful for indigenous callers and test fixtures.
  if(!input.empty()&&input.begin().value().is_object()&&input.begin().value().contains("command"))return input;
  return Json::object();
}
void merge(Json& into,const Json& from){
  auto values=serversFrom(from);
  for(auto it=values.begin();it!=values.end();++it)into[it.key()]=it.value();
}
bool executable(const std::string& command){
  if(command.empty())return false;
  if(command.find('/')!=std::string::npos||command.find('\\')!=std::string::npos)
    return std::filesystem::exists(command);
  auto* raw=std::getenv("PATH");if(!raw)return false;
  std::string path=raw;
#ifdef _WIN32
  constexpr char separator=';';
#else
  constexpr char separator=':';
#endif
  std::size_t pos=0;
  while(pos<=path.size()){
    auto next=path.find(separator,pos);if(next==std::string::npos)next=path.size();
    auto candidate=std::filesystem::path(path.substr(pos,next-pos))/command;
    if(std::filesystem::exists(candidate))return true;
#ifdef _WIN32
    if(std::filesystem::exists(candidate.string()+".exe"))return true;
#endif
    pos=next+1;
  }
  return false;
}
Json builtins(){
  Json values=Json::object();
  if(executable("clangd"))values["clangd"]={{"command","clangd"},{"args",Json::array({"--background-index"})},
    {"extensionToLanguage",{{".c","c"},{".h","c"},{".cc","cpp"},{".cpp","cpp"},
      {".cxx","cpp"},{".hpp","cpp"},{".hh","cpp"},{".hxx","cpp"}}}};
  if(executable("typescript-language-server"))values["typescript"]={{"command","typescript-language-server"},
    {"args",Json::array({"--stdio"})},{"extensionToLanguage",{{".ts","typescript"},
      {".tsx","typescriptreact"},{".js","javascript"},{".jsx","javascriptreact"}}}};
  if(executable("pyright-langserver"))values["pyright"]={{"command","pyright-langserver"},
    {"args",Json::array({"--stdio"})},{"extensionToLanguage",{{".py","python"}}}};
  else if(executable("pylsp"))values["pylsp"]={{"command","pylsp"},
    {"extensionToLanguage",{{".py","python"}}}};
  return values;
}
Json initParams(const ServerConfig& config,const std::filesystem::path& roost){
  auto root=config.roostFolder.empty()?roost:std::filesystem::path(config.roostFolder);
  auto uri=fileUri(root);
  Json textDocument=Json::object();
  textDocument["synchronization"]={{"dynamicRegistration",false},{"willSave",false},
    {"willSaveWaitUntil",false},{"didSave",true}};
  textDocument["publishDiagnostics"]={{"relatedInformation",true},
    {"tagSupport",{{"valueSet",Json::array({1,2})}}},{"versionSupport",false},
    {"codeDescriptionSupport",true},{"dataSupport",false}};
  textDocument["hover"]={{"dynamicRegistration",false},
    {"contentFormat",Json::array({"markdown","plaintext"})}};
  textDocument["definition"]={{"dynamicRegistration",false},{"linkSupport",true}};
  textDocument["references"]={{"dynamicRegistration",false}};
  textDocument["documentSymbol"]={{"dynamicRegistration",false},
    {"hierarchicalDocumentSymbolSupport",true}};
  textDocument["callHierarchy"]={{"dynamicRegistration",false}};
  Json capabilities={{"workspace",{{"configuration",false},{"workspaceFolders",false}}},
    {"textDocument",std::move(textDocument)},
    {"general",{{"positionEncodings",Json::array({"utf-16"})}}}};
  return {{"processId",uv_os_getpid()},{"initializationOptions",config.initializationOptions},
    {"workspaceFolders",Json::array({Json{{"uri",uri},{"name",root.filename().string()}}})},
    {"rootPath",root.string()},{"rootUri",uri},{"capabilities",std::move(capabilities)}};
}
}
std::vector<ServerConfig> loadServerConfigs(const std::filesystem::path& roost,const Json& supplied){
  Json combined=Json::object();
  if(const auto* configured=std::getenv("BATBOX_CONFIG_DIR");configured&&*configured){
    merge(combined,readConfig(std::filesystem::path(configured)/"lsp.json"));
  }else{
    const auto* userDirectory=std::getenv("HOME");
    if(!userDirectory||!*userDirectory)userDirectory=std::getenv("USERPROFILE");
    if(userDirectory&&*userDirectory)
      merge(combined,readConfig(std::filesystem::path(userDirectory)/".batbox"/"lsp.json"));
  }
  merge(combined,readConfig(roost/".batbox"/"lsp.json"));
  merge(combined,supplied);
  auto fallback=builtins();
  for(auto it=fallback.begin();it!=fallback.end();++it)
    if(!combined.contains(it.key()))combined[it.key()]=it.value();
  std::vector<ServerConfig> result;
  for(auto it=combined.begin();it!=combined.end();++it){
    try{
      const auto& item=it.value();if(!item.is_object()||item.value("disabled",false))continue;
      ServerConfig config;config.name=it.key();config.command=item.at("command").get<std::string>();
      if(config.command.empty())continue;
      if(item.contains("args")&&item.at("args").is_array())for(const auto& arg:item.at("args"))config.args.push_back(arg.get<std::string>());
      if(item.contains("env")&&item.at("env").is_object())for(auto env=item.at("env").begin();env!=item.at("env").end();++env)
        config.env[env.key()]=env.value().get<std::string>();
      config.roostFolder=item.value("workspaceFolder",std::string{});
      config.initializationOptions=item.value("initializationOptions",Json::object());
      config.startupTimeoutMs=item.value("startupTimeout",15000);
      config.maxRestarts=item.value("maxRestarts",3);
      if(item.contains("restartOnCrash")||item.contains("shutdownTimeout"))continue;
      if(!item.contains("extensionToLanguage")||!item.at("extensionToLanguage").is_object())continue;
      for(auto ext=item.at("extensionToLanguage").begin();ext!=item.at("extensionToLanguage").end();++ext)
        config.extensionToLanguage[lower(ext.key())]=ext.value().get<std::string>();
      if(!config.extensionToLanguage.empty())result.push_back(std::move(config));
    }catch(...){/* A malformed optional server cannot disable the other servers. */}
  }
  return result;
}
Manager::Manager(std::filesystem::path roost,std::vector<ServerConfig> configs,
    std::shared_ptr<Diagnostics> diagnostics):
  roost_(std::filesystem::absolute(std::move(roost)).lexically_normal()),
  diagnostics_(std::move(diagnostics)){
  for(auto& config:configs){
    auto name=config.name;
    for(const auto& [ext,language]:config.extensionToLanguage){(void)language;extensions_[lower(ext)].push_back(name);}
    Server server;
    server.config=std::move(config);
    servers_.emplace(name,std::move(server));
  }
}
Manager::~Manager(){shutdown();}
const Manager::Server* Manager::get(const std::string& path)const{
  auto ext=lower(std::filesystem::path(path).extension().string());
  auto found=extensions_.find(ext);
  if(found==extensions_.end()||found->second.empty())return nullptr;
  auto server=servers_.find(found->second.front());return server==servers_.end()?nullptr:&server->second;
}
Manager::Server* Manager::get(const std::string& path){return const_cast<Server*>(std::as_const(*this).get(path));}
bool Manager::hasServer(const std::string& path)const{return get(path)!=nullptr;}
void Manager::initialize(Server& server){
  if(server.client)return;
  if(server.crashRecoveryCount>server.config.maxRestarts)
    throw std::runtime_error("LSP server '"+server.config.name+"' exceeded max crash recovery attempts");
  std::vector<std::string> command{server.config.command};
  command.insert(command.end(),server.config.args.begin(),server.config.args.end());
  auto cwd=server.config.roostFolder.empty()?roost_.string():server.config.roostFolder;
  try{
    auto client=std::make_unique<Client>(server.config.name,command,server.config.env,cwd,
      [diagnostics=diagnostics_,name=server.config.name](const std::string& method,const Json& params){
        if(method=="textDocument/publishDiagnostics")diagnostics->publish(name,params);
      });
    client->request("initialize",initParams(server.config,roost_),
      std::chrono::milliseconds(std::max(1000,server.config.startupTimeoutMs)));
    client->notify("initialized",Json::object());
    client->markInitialized();
    server.client=std::move(client);server.error=false;server.crashRecoveryCount=0;
  }catch(...){server.error=true;++server.crashRecoveryCount;throw;}
}
Manager::Server* Manager::ensure(const std::string& path){
  auto server=get(path);if(server&&!server->client)initialize(*server);return server;
}
Json Manager::request(const std::string& path,const std::string& method,const Json& params,std::stop_token signal){
  auto server=ensure(path);if(!server)return Json();
  if(auto found=documents_.find(fileUri(path));found!=documents_.end())
    found->second.lastUsed=++documentClock_;
  for(int attempt=0;attempt<=3;++attempt){
    try{return server->client->request(method,params,std::chrono::seconds(15),signal);}
    catch(const std::exception& error){
      if(std::string(error.what()).find("LSP error -32801:")!=std::string::npos&&attempt<3){
        if(signal.stop_requested())throw;
        std::this_thread::sleep_for(std::chrono::milliseconds(500*(1<<attempt)));continue;
      }
      if(std::string(error.what()).find("disconnected")!=std::string::npos||
         std::string(error.what()).find("timed out")!=std::string::npos){
        server->client.reset();server->error=true;++server->crashRecoveryCount;
        for(auto it=documents_.begin();it!=documents_.end();){
          if(it->second.server==server->config.name)it=documents_.erase(it);else ++it;}
      }
      throw;
    }
  }
  throw std::runtime_error("LSP request failed");
}
void Manager::evictDocumentIfFull(){
  constexpr std::size_t maxOpenDocuments=128;
  if(documents_.size()<maxOpenDocuments)return;
  auto oldest=std::min_element(documents_.begin(),documents_.end(),
    [](const auto& left,const auto& right){
      return left.second.lastUsed<right.second.lastUsed;});
  if(oldest==documents_.end())return;
  const auto uri=oldest->first;
  try{
    auto server=servers_.find(oldest->second.server);
    if(server!=servers_.end()&&server->second.client)
      server->second.client->notify("textDocument/didClose",{{"textDocument",{{"uri",uri}}}});
  }catch(...){}
  documents_.erase(oldest);
  if(diagnostics_)diagnostics_->dropFile(uri);
}
void Manager::openFile(const std::string& path,const std::string& content){
  auto server=ensure(path);if(!server)return;
  auto uri=fileUri(path);
  if(auto it=documents_.find(uri);it!=documents_.end()&&it->second.server==server->config.name){
    it->second.lastUsed=++documentClock_;return;
  }
  evictDocumentIfFull();
  auto ext=lower(std::filesystem::path(path).extension().string());
  auto language=server->config.extensionToLanguage.at(ext);
  server->client->notify("textDocument/didOpen",{{"textDocument",{{"uri",uri},{"languageId",language},
    {"version",1},{"text",content}}}});
  documents_[uri]={server->config.name,1,++documentClock_};
}
void Manager::changeFile(const std::string& path,const std::string& content){
  auto server=get(path);if(!server)return;
  auto uri=fileUri(path);
  if(!server->client||!documents_.contains(uri)){openFile(path,content);return;}
  auto& document=documents_.at(uri);
  document.lastUsed=++documentClock_;
  server->client->notify("textDocument/didChange",{{"textDocument",{{"uri",uri},{"version",++document.version}}},
    {"contentChanges",Json::array({Json{{"text",content}}})}});
}
void Manager::saveFile(const std::string& path){
  auto server=get(path);if(!server||!server->client)return;
  if(auto found=documents_.find(fileUri(path));found!=documents_.end())
    found->second.lastUsed=++documentClock_;
  server->client->notify("textDocument/didSave",{{"textDocument",{{"uri",fileUri(path)}}}});
}
void Manager::closeFile(const std::string& path){
  auto server=get(path);auto uri=fileUri(path);
  if(server&&server->client&&documents_.contains(uri))
    server->client->notify("textDocument/didClose",{{"textDocument",{{"uri",uri}}}});
  documents_.erase(uri);
}
bool Manager::isFileOpen(const std::string& path)const{return documents_.contains(fileUri(path));}
void Manager::shutdown()noexcept{
  for(auto& [uri,document]:documents_){
    try{auto it=servers_.find(document.server);if(it!=servers_.end()&&it->second.client)
      it->second.client->notify("textDocument/didClose",{{"textDocument",{{"uri",uri}}}});}catch(...){}
  }
  documents_.clear();
  for(auto& [name,server]:servers_){(void)name;if(server.client){server.client->stop();server.client.reset();}}
  if(diagnostics_)diagnostics_->reset();
}
}
