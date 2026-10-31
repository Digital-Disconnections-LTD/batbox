#include "client_factory/ClientFactory.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using namespace batbox;
std::string bioText(BIO* bio){
  char* data=nullptr;
  const auto length=BIO_get_mem_data(bio,&data);
  return std::string(data,static_cast<std::size_t>(length));
}
struct Certificate {
  std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key{nullptr,EVP_PKEY_free};
  std::unique_ptr<X509,decltype(&X509_free)> cert{nullptr,X509_free};
  std::string certPem,keyPem;
};
Certificate certificate(){
  Certificate result;
  std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)> keyContext(
    EVP_PKEY_CTX_new_id(EVP_PKEY_RSA,nullptr),EVP_PKEY_CTX_free);
  if(!keyContext||EVP_PKEY_keygen_init(keyContext.get())<=0||
      EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext.get(),2048)<=0)
    throw std::runtime_error("TLS key setup failed");
  EVP_PKEY* rawKey=nullptr;
  if(EVP_PKEY_keygen(keyContext.get(),&rawKey)<=0)
    throw std::runtime_error("TLS key generation failed");
  result.key.reset(rawKey);
  result.cert.reset(X509_new());
  if(!result.cert)throw std::runtime_error("TLS certificate setup failed");
  X509_set_version(result.cert.get(),2);
  ASN1_INTEGER_set(X509_get_serialNumber(result.cert.get()),1);
  X509_gmtime_adj(X509_get_notBefore(result.cert.get()),-60);
  X509_gmtime_adj(X509_get_notAfter(result.cert.get()),3600);
  X509_set_pubkey(result.cert.get(),result.key.get());
  std::unique_ptr<X509_NAME,decltype(&X509_NAME_free)> subject(
    X509_NAME_new(),X509_NAME_free);
  auto* name=subject.get();
  if(!name)throw std::runtime_error("TLS subject allocation failed");
  X509_NAME_add_entry_by_txt(name,"CN",MBSTRING_ASC,
    reinterpret_cast<const unsigned char*>("localhost"),-1,-1,0);
  X509_set_subject_name(result.cert.get(),name);
  X509_set_issuer_name(result.cert.get(),name);
  X509V3_CTX extensionContext{};
  X509V3_set_ctx(&extensionContext,result.cert.get(),
    result.cert.get(),nullptr,nullptr,0);
  std::unique_ptr<X509_EXTENSION,decltype(&X509_EXTENSION_free)> extension(
    X509V3_EXT_conf_nid(nullptr,&extensionContext,NID_subject_alt_name,
      const_cast<char*>("DNS:localhost")),X509_EXTENSION_free);
  if(!extension||X509_add_ext(result.cert.get(),extension.get(),-1)!=1||
      X509_sign(result.cert.get(),result.key.get(),EVP_sha256())<=0)
    throw std::runtime_error("TLS certificate signing failed");
  std::unique_ptr<BIO,decltype(&BIO_free)> certOut(BIO_new(BIO_s_mem()),
    BIO_free),keyOut(BIO_new(BIO_s_mem()),BIO_free);
  if(PEM_write_bio_X509(certOut.get(),result.cert.get())!=1||
      PEM_write_bio_PrivateKey(keyOut.get(),result.key.get(),nullptr,
        nullptr,0,nullptr,nullptr)!=1)
    throw std::runtime_error("TLS PEM serialization failed");
  result.certPem=bioText(certOut.get());
  result.keyPem=bioText(keyOut.get());
  return result;
}
int listener(unsigned short& port){
  const int fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd<0)throw std::runtime_error("TLS socket failed");
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||
      listen(fd,1)<0)throw std::runtime_error("TLS listen failed");
  socklen_t length=sizeof(address);
  getsockname(fd,reinterpret_cast<sockaddr*>(&address),&length);
  port=ntohs(address.sin_port);
  return fd;
}
}
int main(){
  using namespace batbox;
  auto identity=certificate();
  std::unique_ptr<SSL_CTX,decltype(&SSL_CTX_free)> context(
    SSL_CTX_new(TLS_server_method()),SSL_CTX_free);
  if(!context||SSL_CTX_use_certificate(context.get(),identity.cert.get())!=1||
      SSL_CTX_use_PrivateKey(context.get(),identity.key.get())!=1||
      X509_STORE_add_cert(SSL_CTX_get_cert_store(context.get()),
        identity.cert.get())!=1)
    throw std::runtime_error("TLS server configuration failed");
  SSL_CTX_set_verify(context.get(),SSL_VERIFY_PEER|
    SSL_VERIFY_FAIL_IF_NO_PEER_CERT,nullptr);
  unsigned short port=0;
  const int fd=listener(port);
  std::string wire;
  std::exception_ptr serverError;
  std::thread server([&]{
    try{
      const int accepted=accept(fd,nullptr,nullptr);
      if(accepted<0)throw std::runtime_error("TLS accept failed");
      std::unique_ptr<SSL,decltype(&SSL_free)> ssl(SSL_new(context.get()),
        SSL_free);
      SSL_set_fd(ssl.get(),accepted);
      if(SSL_accept(ssl.get())<=0)
        throw std::runtime_error("TLS handshake failed");
      char buffer[4096];
      while(wire.find("\r\n\r\n")==std::string::npos){
        const int count=SSL_read(ssl.get(),buffer,sizeof(buffer));
        if(count<=0)throw std::runtime_error("TLS read failed");
        wire.append(buffer,static_cast<std::size_t>(count));
      }
      const std::string body=R"({"id":"secure"})";
      const auto response="HTTP/1.1 200 OK\r\nContent-Type: "
        "application/json\r\nContent-Length: "+
        std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
      if(SSL_write(ssl.get(),response.data(),
          static_cast<int>(response.size()))<=0)
        throw std::runtime_error("TLS write failed");
      SSL_shutdown(ssl.get());
      close(accepted);
    }catch(...){serverError=std::current_exception();}
  });
  client_factory::ClientPlan plan;
  plan.proxyFetchOptions=Json{{"tls",Json{{"cert",identity.certPem},
    {"key",identity.keyPem},{"ca",Json::array({identity.certPem})}}}};
  client_factory::Services services;
  services.directBaseUrl="https://localhost:"+std::to_string(port);
  std::exception_ptr requestError;
  try{
    auto client=client_factory::makeIndigenousDirectClient(plan,services);
    const auto response=client->request(Json{{"model","qwen"},
      {"max_tokens",1},{"messages",Json::array()}},false);
    assert(response.body["id"]=="secure");
  }catch(...){requestError=std::current_exception();}
  server.join();close(fd);
  if(serverError)std::rethrow_exception(serverError);
  if(requestError)std::rethrow_exception(requestError);
  assert(wire.starts_with("POST /v1/messages HTTP/1.1"));
  std::cout<<"native HTTPS mTLS and CA PEM request passed\n";
}
