#include "auth/Credential.hpp"
#include <cmath>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::auth {
namespace {
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
bool enoent(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::system_error& caught){
    return caught.code().value()==ENOENT;
  }catch(...){return false;}
}
}
CredentialReader::CredentialReader(CredentialServices services):
  services_(std::move(services)){
  if(!services_.environment||!services_.platform||!services_.remoteMode||
      !services_.readFileSync||!services_.mkdirRecursive||
      !services_.writeFileSync||!services_.debug||
      services_.paths.tokenDirectory.empty()||
      services_.paths.oauthFile.empty()||services_.paths.apiKeyFile.empty())
    throw std::invalid_argument("Missing credential reader binding");
}
void CredentialReader::maybePersistTokenForSubprocesses(
    const std::string& path,const std::string& token,
    const std::string& tokenName){
  if(!services_.remoteMode())return;
  try{
    services_.mkdirRecursive(services_.paths.tokenDirectory,0700);
    services_.writeFileSync(path,token,0600);
    services_.debug("Persisted "+tokenName+" to "+path+
      " for subprocess access","debug");
  }catch(...){
    services_.debug("Failed to persist "+tokenName+
      " to disk (non-fatal): "+errorMessage(std::current_exception()),
      "error");
  }
}
std::optional<std::string> CredentialReader::readTokenFromWellKnownFile(
    const std::string& path,const std::string& tokenName){
  try{
    const auto token=value::trim(services_.readFileSync(path));
    if(token.empty())return std::nullopt;
    services_.debug("Read "+tokenName+" from well-known file "+path,
      "debug");
    return token;
  }catch(...){
    const auto error=std::current_exception();
    if(!enoent(error))
      services_.debug("Failed to read "+tokenName+" from "+path+": "+
        errorMessage(error),"debug");
    return std::nullopt;
  }
}
std::optional<std::string> CredentialReader::getCredentialFromFd(
    const std::string& envVar,const std::string& wellKnownPath,
    const std::string& label,
    std::optional<std::optional<std::string>>& stashed){
  if(stashed)return *stashed;
  const auto fdEnv=services_.environment(envVar);
  if(!fdEnv||fdEnv->empty()){
    auto fromFile=readTokenFromWellKnownFile(wellKnownPath,label);
    stashed=fromFile;return fromFile;
  }
  const auto number=value::parseInt10(*fdEnv);
  if(std::isnan(number)){
    services_.debug(envVar+" must be a valid file descriptor number, got: "+
      *fdEnv,"error");
    stashed=std::nullopt;return std::nullopt;
  }
  try{
    const auto platform=services_.platform();
    const auto path=(platform=="darwin"||platform=="freebsd"
      ?"/dev/fd/":"/proc/self/fd/")+value::numberToString(number);
    const auto token=value::trim(services_.readFileSync(path));
    if(token.empty()){
      services_.debug("File descriptor contained empty "+label,"error");
      stashed=std::nullopt;return std::nullopt;
    }
    services_.debug("Successfully read "+label+
      " from file descriptor "+value::numberToString(number),"debug");
    stashed=token;
    maybePersistTokenForSubprocesses(wellKnownPath,token,label);
    return token;
  }catch(...){
    services_.debug("Failed to read "+label+" from file descriptor "+
      value::numberToString(number)+": "+
      errorMessage(std::current_exception()),"error");
    auto fromFile=readTokenFromWellKnownFile(wellKnownPath,label);
    stashed=fromFile;return fromFile;
  }
}
std::optional<std::string> CredentialReader::getOAuthTokenFromFileDescriptor(){
  return getCredentialFromFd(services_.paths.oauthEnv,
    services_.paths.oauthFile,"OAuth token",oauthStashed_);
}
std::optional<std::string> CredentialReader::getApiKeyFromFileDescriptor(){
  return getCredentialFromFd(services_.paths.apiKeyEnv,
    services_.paths.apiKeyFile,"API key",apiKeyStashed_);
}
void CredentialReader::clearStashedFileDescriptors(){
  oauthStashed_.reset();apiKeyStashed_.reset();
}
}
