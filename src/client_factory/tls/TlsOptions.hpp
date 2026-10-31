#pragma once
#include "client_factory/ClientFactory.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::client_factory::tls {
struct Services {
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::function<std::string(const std::string&)> readFileUtf8;
  std::function<void()> prepareCertificateProvider;
  std::function<std::vector<std::string>()> bundledRootCertificates;
  std::function<std::optional<std::vector<std::string>>()>
    systemCaCertificates;
  std::function<std::shared_ptr<void>(const Json&)> makeNodeBat;
  std::function<void()> clearMTLSBatStash;
  std::function<void(const std::string&,const std::string&)> log;
  bool bunRuntime=true;
  bool systemCaApiAvailable=true;
  std::string optionsKey="BATBOX_TLS_OPTIONS";
  std::string extraCaKey="BATBOX_EXTRA_CA_CERTS";
  std::string certKey="BATBOX_CLIENT_CERT";
  std::string keyKey="BATBOX_CLIENT_KEY";
  std::string passphraseKey="BATBOX_CLIENT_KEY_PASSPHRASE";
};
class TlsOptions {
public:
  explicit TlsOptions(Services services);
  bool hasRuntimeOption(const std::string& flag)const;
  std::shared_ptr<const Json> getMTLSConfig();
  std::shared_ptr<const std::vector<std::string>> getCACertificates();
  NetworkOptions getTLSFetchOptions();
  std::optional<Json> getWebSocketTLSOptions();
  void clearCACertsStash();
  void clearMTLSStash();
private:
  Services services_;
  std::optional<std::shared_ptr<const Json>> mtlsStash_;
  std::optional<std::shared_ptr<const std::vector<std::string>>> caStash_;
  std::optional<std::string> env(const std::string& key)const;
  void debug(const std::string& message,
    const std::string& level="debug")const;
};
}
