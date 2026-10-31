#include "client_factory/tls/IndigenousCerts.hpp"
#include "client_factory/tls/IndigenousRoots.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace batbox::client_factory::tls {
namespace {
std::string readFile(const std::string& path){
  std::ifstream stream(runtime::pathFromUtf8(path),std::ios::binary);
  if(!stream)throw std::runtime_error("Cannot read certificate file: "+path);
  std::string value((std::istreambuf_iterator<char>(stream)),
    std::istreambuf_iterator<char>());
  if(!stream.eof()&&stream.fail())
    throw std::runtime_error("Cannot finish certificate file: "+path);
  return value;
}
}
Services makeIndigenousTlsServices(const std::string& caBundlePath,
    std::function<void(const std::string&,const std::string&)> log){
  Services services;
  services.environment=[](const std::string& key)->
      std::optional<std::string>{
#ifdef _WIN32
    return runtime::environmentUtf8Optional(key.c_str());
#else
    const char* value=std::getenv(key.c_str());
    return value?std::optional<std::string>(value):std::nullopt;
#endif
  };
  services.readFileUtf8=readFile;
  services.bundledRootCertificates=[caBundlePath]{
    return std::vector<std::string>{readFile(caBundlePath.empty()?
      indigenousCaBundlePath():caBundlePath)};};
  services.systemCaCertificates=[caBundlePath]()->
      std::optional<std::vector<std::string>>{
#ifdef _WIN32
    (void)caBundlePath;
    return indigenousWindowsRootCertificates();
#else
    return std::vector<std::string>{readFile(caBundlePath.empty()?
      indigenousCaBundlePath():caBundlePath)};
#endif
  };
  services.bunRuntime=true;
#ifdef __APPLE__
  services.systemCaApiAvailable=false;
#else
  services.systemCaApiAvailable=true;
#endif
  services.log=std::move(log);
  return services;
}
}
