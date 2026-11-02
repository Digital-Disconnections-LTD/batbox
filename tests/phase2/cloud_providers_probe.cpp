#include "providers/aws/BedrockClient.hpp"
#include "providers/aws/EventStream.hpp"
#include "providers/gcp/VertexClient.hpp"
#include "transport/Errors.hpp"
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

using namespace batbox;
using namespace batbox::providers;

namespace {
std::string slurp(const std::string& path) {
  std::ifstream input(path,std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}
Json describe(const std::exception& error) {
  Json out{{"message",error.what()}};
  if (const auto* api = dynamic_cast<const transport::APIError*>(&error)) {
    out["kind"] = api->kind();
    if (api->status) out["status"] = *api->status;
    out["body"] = api->body;
  } else if (dynamic_cast<const aws::EventStreamError*>(&error)) {
    out["kind"] = "EventStreamError";
  } else out["kind"] = "exception";
  return out;
}
int eventStream(const std::string& file, std::size_t chunk) {
  const auto bytes = slurp(file);
  Json out{{"events",Json::array()},{"messages",Json::array()}};
  aws::EventStreamDecoder decoder;
  aws::BedrockStreamTranslator translator;
  try {
    for (std::size_t at = 0; at < bytes.size(); at += chunk) {
      for (const auto& message : decoder.decode(std::string_view(bytes).substr(at,chunk))) {
        Json headers = Json::object();
        for (const auto& [name,text] : message.headers) headers[name] = text;
        out["messages"].push_back(Json{{"headers",headers},{"payload_bytes",message.payload.size()}});
        translator.handle(message,[&](const Json& event) { out["events"].push_back(event); });
      }
    }
    if (decoder.pending())
      throw transport::APIConnectionError("Bedrock event stream ended inside a message ("+std::to_string(decoder.pendingBytes())+" bytes left)");
  } catch (const std::exception& error) { out["error"] = describe(error); }
  std::cout << value::stringify(out) << '\n';
  return 0;
}
Json run(cloud::WireClient& client, const std::string& action, const Json& body, int repeat) {
  Json results = Json::array();
  for (int i = 0; i < repeat; ++i) {
    Json result{{"events",Json::array()}};
    try {
      if (action == "models") result["models"] = client.listModels();
      else {
        bool early = false;
        const auto response = client.request(body,action == "stream",[&](const Json& event) { result["events"].push_back(event); },{},
                                             [&](const transport::Response&) { early = true; });
        result["status"] = response.status;
        result["headers_reported"] = early;
        result["request_id"] = response.headers.value("request-id","");
        result["body"] = response.body;
      }
    } catch (const std::exception& error) { result["error"] = describe(error); }
    results.push_back(std::move(result));
  }
  return results;
}
int bedrock(const std::string& action, const Json& config, const Json& body, int repeat) {
  aws::BedrockConfig settings;
  settings.region = config.value("region","");
  settings.baseUrl = config.value("base_url","");
  settings.bearerToken = config.value("bearer_token","");
  if (config.contains("headers")) settings.headers = config["headers"];
  aws::BedrockClient client(settings);
  std::cout << value::stringify(Json{{"kind",client.kind()},{"results",run(client,action,body,repeat)}}) << '\n';
  return 0;
}
int vertex(const std::string& action, const Json& config, const Json& body, int repeat) {
  gcp::VertexConfig settings;
  settings.project = config.value("project","");
  settings.region = config.value("region","");
  settings.baseUrl = config.value("base_url","");
  settings.bearerToken = config.value("bearer_token","");
  if (config.contains("models")) settings.models = config["models"].get<std::vector<std::string>>();
  gcp::GoogleAuthOptions auth;
  auth.credentialsFile = config.value("credentials_file","");
  if (config.contains("gcloud_command")) auth.gcloudCommand = config["gcloud_command"];
  gcp::VertexClient client(settings,auth);
  std::cout << value::stringify(Json{{"kind",client.kind()},{"results",run(client,action,body,repeat)}}) << '\n';
  return 0;
}
int generateKey(const std::string& path) {
  std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)> context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA,nullptr),EVP_PKEY_CTX_free);
  EVP_PKEY* raw = nullptr;
  if (EVP_PKEY_keygen_init(context.get()) != 1 || EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(),2048) != 1 ||
      EVP_PKEY_keygen(context.get(),&raw) != 1) return 1;
  std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(raw,EVP_PKEY_free);
  std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()),BIO_free);
  PEM_write_bio_PrivateKey(bio.get(),key.get(),nullptr,nullptr,0,nullptr,nullptr);
  char* data = nullptr;
  const auto length = BIO_get_mem_data(bio.get(),&data);
  std::ofstream(path,std::ios::binary).write(data,length);
  BIGNUM* n = nullptr;
  BIGNUM* e = nullptr;
  EVP_PKEY_get_bn_param(key.get(),OSSL_PKEY_PARAM_RSA_N,&n);
  EVP_PKEY_get_bn_param(key.get(),OSSL_PKEY_PARAM_RSA_E,&e);
  char* nHex = BN_bn2hex(n);
  char* eHex = BN_bn2hex(e);
  std::cout << value::stringify(Json{{"n",nHex},{"e",eHex}}) << '\n';
  OPENSSL_free(nHex);
  OPENSSL_free(eHex);
  BN_free(n);
  BN_free(e);
  return 0;
}
} // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: cloud-providers-probe eventstream FILE CHUNK | bedrock|vertex ACTION CONFIG [BODY] [REPEAT] | genkey PATH\n"; return 2; }
  const std::string mode = argv[1];
  if (mode == "eventstream" && argc >= 4) return eventStream(argv[2],std::stoul(argv[3]));
  if (mode == "genkey") return generateKey(argv[2]);
  if ((mode == "bedrock" || mode == "vertex") && argc >= 4) {
    const auto config = value::parse(slurp(argv[3]));
    const auto body = argc >= 5 ? value::parse(slurp(argv[4])) : Json::object();
    const int repeat = argc >= 6 ? std::stoi(argv[5]) : 1;
    return mode == "bedrock" ? bedrock(argv[2],config,body,repeat) : vertex(argv[2],config,body,repeat);
  }
  std::cerr << "unknown mode\n";
  return 2;
}
