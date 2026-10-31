#include "auth/Credential.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <iostream>
#include <system_error>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),values=Json::array();
    const auto files=test.value("files",Json::object());
    auth::CredentialServices services;
    services.paths={"BATBOX_OAUTH_TOKEN_FILE_DESCRIPTOR",
      "BATBOX_API_KEY_FILE_DESCRIPTOR","/remote/.oauth_token",
      "/remote/.api_key","/remote"};
    services.environment=[&](const std::string& key){
      trace.push_back({"env",key});
      const char* field=key==services.paths.oauthEnv?"oauthFd":"apiFd";
      if(!test.contains(field)||test[field].is_null())
        return std::optional<std::string>{};
      return std::optional(test[field].get<std::string>());
    };
    services.platform=[&](){return test.value("platform",std::string("linux"));};
    services.remoteMode=[&](){trace.push_back({"env","BATBOX_REMOTE"});
      return test.value("remote",false);};
    services.readFileSync=[&](const std::string& path){
      trace.push_back({"read",path});
      if(!files.contains(path))throw std::system_error(ENOENT,
        std::generic_category(),"read credential");
      return files[path].get<std::string>();
    };
    services.mkdirRecursive=[&](const std::string& path,int mode){
      trace.push_back({"mkdir",path,mode});};
    services.writeFileSync=[&](const std::string& path,
        const std::string& token,int mode){
      trace.push_back({"write",path,token,mode});};
    services.debug=[&](const std::string& message,
        const std::string& level){
      trace.push_back({"debug",message,level});};
    auth::CredentialReader reader(services);
    for(const auto& action:test["actions"]){
      const auto op=action.get<std::string>();
      if(op=="oauth"){
        const auto value=reader.getOAuthTokenFromFileDescriptor();
        values.push_back(value?Json(*value):Json(nullptr));
      }else if(op=="api"){
        const auto value=reader.getApiKeyFromFileDescriptor();
        values.push_back(value?Json(*value):Json(nullptr));
      }else if(op=="read"){
        const auto value=reader.readTokenFromWellKnownFile(
          "/remote/.oauth_token","OAuth token");
        values.push_back(value?Json(*value):Json(nullptr));
      }else if(op=="persist")reader.maybePersistTokenForSubprocesses(
        "/remote/.oauth_token","token","OAuth token");
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})<<'\n';
  }
}
