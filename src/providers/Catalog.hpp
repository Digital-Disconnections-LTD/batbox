#pragma once
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace batbox::providers {
// One model a provider serves, trimmed from models.dev.
struct CatalogModel {
  std::string id,name;
  std::size_t context=0,output=0;
  bool toolCall=false,reasoning=false;
  std::vector<std::string> inputs;
};
// One provider preset. kind is the wire kind an endpoint record stores in
// "provider": anthropic, openai-chat, bedrock, vertex, or unsupported.
struct CatalogProvider {
  std::string id,name,kind,baseUrl,messagesPath,modelsPath;
  std::string auth="key",authHeader="authorization";
  std::string anthropicNative="unverified",doc,note,reason;
  std::vector<std::string> keyEnv,configEnv,aliases,baseUrlNeeds;
  std::vector<int> ports;
  Json headers=Json::object(),quirks;
  bool local=false;
  std::vector<CatalogModel> models;
  bool supported()const{return kind!="unsupported";}
  // Sign-in flows Batbox cannot run yet.
  bool comingSoon()const{return auth=="oauth-device";}
  const CatalogModel* model(const std::string& id)const;
};
struct Catalog {
  std::vector<CatalogProvider> providers;
  std::size_t defaultContext=131072,defaultOutput=16384;
  const CatalogProvider* find(const std::string& idOrAlias)const;
};
// The bundled catalog, parsed once on first use.
const Catalog& bundledCatalog();
Catalog parseCatalog(const Json& document);
std::string bundledCatalogText();
// Defined in the generated file.
std::size_t catalogChunkCount();
const char* catalogChunk(std::size_t index);
std::size_t catalogBytes();
// "Cloud", "Local servers" or "Unsupported yet".
std::string catalogSection(const CatalogProvider& provider);
// "Anthropic Messages", "OpenAI Chat (translated)", "AWS Bedrock", ...
std::string kindLabel(const std::string& kind);
// The same in a word or two for narrow lists: "Anthropic", "OpenAI chat".
std::string shortKindLabel(const std::string& kind);
// "200K context · 64K output · tools · reasoning"
std::string describeModel(const CatalogModel& model);
std::string compactTokens(std::size_t tokens);
} // namespace batbox::providers
