#include <unistd.h>
#include "client_factory/tls/IndigenousCerts.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void write(const std::filesystem::path& path,const std::string& value){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);
  if(!file)throw std::runtime_error("Cannot create TLS fixture");
  file<<value;
}
}
int main(){
  using namespace batbox::client_factory;
  char pattern[]="/tmp/batbox-client-tls-XXXXXX";
  char* directory=mkdtemp(pattern);
  if(!directory)throw std::runtime_error("mkdtemp failed");
  const auto root=std::filesystem::path(directory);
  const auto base=root/"base.pem",extra=root/"extra.pem",
    cert=root/"client.crt",key=root/"client.key";
  write(base,"BASE-PEM\n");
  write(extra,"EXTRA-PEM\n");
  write(cert,"CERT-PEM\n");
  write(key,"KEY-PEM\n");
  setenv("BATBOX_CLIENT_CERT",cert.c_str(),1);
  setenv("BATBOX_CLIENT_KEY",key.c_str(),1);
  setenv("BATBOX_EXTRA_CA_CERTS",extra.c_str(),1);
  setenv("BATBOX_TLS_OPTIONS","",1);
  tls::TlsOptions options(tls::makeIndigenousTlsServices(base.string()));
  const auto first=options.getTLSFetchOptions();
  assert(first.values["tls"]["cert"]=="CERT-PEM\n");
  assert(first.values["tls"]["key"]=="KEY-PEM\n");
  assert(first.values["tls"]["ca"]==
    batbox::Json::array({"BASE-PEM\n","EXTRA-PEM\n"}));
  write(extra,"UPDATED-PEM\n");
  assert(options.getTLSFetchOptions().values["tls"]["ca"][1]==
    "EXTRA-PEM\n");
  options.clearCACertsStash();
  assert(options.getTLSFetchOptions().values["tls"]["ca"][1]==
    "UPDATED-PEM\n");
  unsetenv("BATBOX_CLIENT_CERT");
  unsetenv("BATBOX_CLIENT_KEY");
  unsetenv("BATBOX_EXTRA_CA_CERTS");
  unsetenv("BATBOX_TLS_OPTIONS");
  std::filesystem::remove_all(root);
  std::cout<<"native TLS file loading and cache invalidation passed\n";
}
