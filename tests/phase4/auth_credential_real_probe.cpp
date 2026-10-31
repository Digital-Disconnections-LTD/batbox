#include "auth/Credential.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void require(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
}
int main(){
  using namespace batbox::auth;
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-credential-"+std::to_string(::getpid())+"-"+
      std::to_string(stamp));
  try{
    int pipeFds[2]{};
    if(::pipe(pipeFds)!=0)throw std::runtime_error("pipe failed");
    const std::string token="  native-token \n";
    if(::write(pipeFds[1],token.data(),token.size())!=
        static_cast<ssize_t>(token.size()))
      throw std::runtime_error("pipe write failed");
    ::close(pipeFds[1]);
    bool useFd=true;
    CredentialPaths paths;
    paths.tokenDirectory=(root/"nested"/"remote").string();
    paths.oauthFile=(root/"nested"/"remote"/".oauth_token").string();
    paths.apiKeyFile=(root/"nested"/"remote"/".api_key").string();
    auto services=indigenousCredentialServices(paths,
      [&](const std::string& key)->std::optional<std::string>{
        if(useFd&&key==paths.oauthEnv)return std::to_string(pipeFds[0]);
        return std::nullopt;
      },[](){return true;},
      [](const std::string&,const std::string&){});
    CredentialReader reader(std::move(services));
    require(reader.getOAuthTokenFromFileDescriptor()=="native-token",
      "FD token read failed");
    require(reader.getOAuthTokenFromFileDescriptor()=="native-token",
      "FD token cache failed");
    ::close(pipeFds[0]);
    std::ifstream input(paths.oauthFile,std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(input)),{});
    require(saved=="native-token","persisted token mismatch");
    struct stat directory{},file{};
    require(::stat(paths.tokenDirectory.c_str(),&directory)==0&&
      (directory.st_mode&077)==0,"credential directory mode mismatch");
    require(::stat(paths.oauthFile.c_str(),&file)==0&&
      (file.st_mode&077)==0,"credential file mode mismatch");
    useFd=false;reader.clearStashedFileDescriptors();
    require(reader.getOAuthTokenFromFileDescriptor()=="native-token",
      "well-known file fallback failed");
    std::filesystem::remove_all(root);
    std::cout<<"Native credential FD/file fixture passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
