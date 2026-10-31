#include "client_factory/tls/IndigenousRoots.hpp"
#include "runtime/IndigenousPath.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <set>
#endif

namespace batbox::client_factory::tls {
namespace {
bool usableBundle(const std::filesystem::path& path){
  std::error_code error;
  if(!std::filesystem::is_regular_file(path,error))return false;
  const auto size=std::filesystem::file_size(path,error);
  return !error&&size>0;
}
#ifdef _WIN32
std::filesystem::path executableDirectory(){
  std::wstring buffer(512,L'\0');
  while(buffer.size()<=65536){
    const auto size=GetModuleFileNameW(nullptr,buffer.data(),
      static_cast<DWORD>(buffer.size()));
    if(!size)throw std::runtime_error("Cannot locate native TLS runtime");
    if(size<buffer.size()){
      buffer.resize(size);return std::filesystem::path(buffer).parent_path();
    }
    buffer.resize(buffer.size()*2);
  }
  throw std::runtime_error("Native TLS runtime path is too long");
}
#endif
}
std::string indigenousCaBundlePath(){
#ifdef _WIN32
  // MSYS2 relocates its OpenSSL trust data relative to the installed bin tree;
  // CURLINFO_CAINFO still reports its compile-time /ucrt64 prefix.
  const auto bin=executableDirectory();
  for(const auto& path:{bin.parent_path()/"etc/ssl/certs/ca-bundle.crt",
      bin/"curl-ca-bundle.crt"})
    if(usableBundle(path))return runtime::pathToUtf8(path);
#endif
  static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
  if(initialized!=CURLE_OK)throw std::runtime_error("TLS runtime initialization failed");
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> client(
    curl_easy_init(),curl_easy_cleanup);
  if(!client)throw std::runtime_error("Cannot inspect native TLS runtime");
#if LIBCURL_VERSION_NUM >= 0x075400
  char* path=nullptr;
  if(curl_easy_getinfo(client.get(),CURLINFO_CAINFO,&path)==CURLE_OK&&path&&
      usableBundle(runtime::pathFromUtf8(path)))return path;
#endif
#ifndef _WIN32
  for(const auto* path:{"/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt","/etc/ssl/cert.pem"})
    if(usableBundle(runtime::pathFromUtf8(path)))return path;
#endif
  throw std::runtime_error("Native CA bundle is unavailable; install the runtime certificate data");
}
#ifdef _WIN32
std::optional<std::vector<std::string>> indigenousWindowsRootCertificates(){
  std::set<std::string> roots;
  for(const DWORD location:{CERT_SYSTEM_STORE_CURRENT_USER,
      CERT_SYSTEM_STORE_LOCAL_MACHINE}){
    const auto store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,
      location|CERT_STORE_READONLY_FLAG|CERT_STORE_OPEN_EXISTING_FLAG,L"ROOT");
    if(!store)continue;
    PCCERT_CONTEXT cert=nullptr;
    try{
      while((cert=CertEnumCertificatesInStore(store,cert))){
        constexpr DWORD format=CRYPT_STRING_BASE64HEADER|CRYPT_STRING_NOCR;
        DWORD size=0;
        if(!CryptBinaryToStringA(cert->pbCertEncoded,cert->cbCertEncoded,
            format,nullptr,&size))continue;
        std::string pem(size,'\0');
        if(!CryptBinaryToStringA(cert->pbCertEncoded,cert->cbCertEncoded,
            format,pem.data(),&size))continue;
        while(!pem.empty()&&pem.back()=='\0')pem.pop_back();
        roots.insert(std::move(pem));
      }
    }catch(...){
      if(cert)CertFreeCertificateContext(cert);
      CertCloseStore(store,0);throw;
    }
    CertCloseStore(store,0);
  }
  return std::vector<std::string>(roots.begin(),roots.end());
}
#endif
}
