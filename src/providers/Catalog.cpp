#include "providers/Catalog.hpp"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <stdexcept>

namespace batbox::providers {
namespace {
std::string text(const Json& row,const char* key){
  const auto found=row.find(key);
  return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};
}
std::vector<std::string> strings(const Json& row,const char* key){
  std::vector<std::string> result;
  const auto found=row.find(key);
  if(found==row.end()||!found->is_array())return result;
  for(const auto& item:*found)if(item.is_string())result.push_back(item.get<std::string>());
  return result;
}
std::string lowered(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
CatalogModel parseModel(const Json& row){
  if(!row.is_array()||row.size()<5)throw std::invalid_argument("Catalog model row is malformed");
  CatalogModel model;
  model.id=row.at(0).get<std::string>();
  model.name=row.at(1).get<std::string>();
  if(model.name.empty())model.name=model.id;
  model.context=row.at(2).get<std::size_t>();
  model.output=row.at(3).get<std::size_t>();
  for(const char flag:row.at(4).get<std::string>()){
    switch(flag){
      case 't':model.toolCall=true;break;
      case 'r':model.reasoning=true;break;
      case 'i':model.inputs.push_back("image");break;
      case 'p':model.inputs.push_back("pdf");break;
      case 'a':model.inputs.push_back("audio");break;
      case 'v':model.inputs.push_back("video");break;
      default:break;
    }
  }
  return model;
}
}

const CatalogModel* CatalogProvider::model(const std::string& id)const{
  for(const auto& candidate:models)if(candidate.id==id)return &candidate;
  return nullptr;
}

const CatalogProvider* Catalog::find(const std::string& idOrAlias)const{
  const auto wanted=lowered(idOrAlias);
  for(const auto& provider:providers)if(lowered(provider.id)==wanted)return &provider;
  for(const auto& provider:providers)
    for(const auto& alias:provider.aliases)if(lowered(alias)==wanted)return &provider;
  return nullptr;
}

Catalog parseCatalog(const Json& document){
  if(!document.is_object()||!document.contains("providers")||!document.at("providers").is_array())
    throw std::invalid_argument("Provider catalog has no providers array");
  Catalog catalog;
  if(document.contains("defaults")){
    catalog.defaultContext=document.at("defaults").value("context",catalog.defaultContext);
    catalog.defaultOutput=document.at("defaults").value("output",catalog.defaultOutput);
  }
  for(const auto& row:document.at("providers")){
    CatalogProvider provider;
    provider.id=text(row,"id");provider.name=text(row,"name");
    provider.kind=text(row,"kind");provider.baseUrl=text(row,"base_url");
    provider.messagesPath=text(row,"messages_path");provider.modelsPath=text(row,"models_path");
    if(auto value=text(row,"auth");!value.empty())provider.auth=value;
    if(auto value=text(row,"auth_header");!value.empty())provider.authHeader=value;
    if(auto value=text(row,"anthropic_native");!value.empty())provider.anthropicNative=value;
    provider.doc=text(row,"doc");provider.note=text(row,"note");provider.reason=text(row,"reason");
    provider.keyEnv=strings(row,"key_env");provider.configEnv=strings(row,"config_env");
    provider.aliases=strings(row,"aliases");provider.baseUrlNeeds=strings(row,"base_url_needs");
    if(row.contains("ports"))for(const auto& port:row.at("ports"))provider.ports.push_back(port.get<int>());
    if(row.contains("headers")&&row.at("headers").is_object())provider.headers=row.at("headers");
    if(row.contains("quirks")&&row.at("quirks").is_object())provider.quirks=row.at("quirks");
    provider.local=row.value("local",false);
    if(provider.id.empty()||provider.name.empty()||provider.kind.empty())
      throw std::invalid_argument("Provider catalog row lacks id, name or kind");
    if(row.contains("models"))for(const auto& model:row.at("models"))
      provider.models.push_back(parseModel(model));
    catalog.providers.push_back(std::move(provider));
  }
  return catalog;
}

std::string bundledCatalogText(){
  std::string result;
  result.reserve(catalogBytes());
  for(std::size_t i=0;i<catalogChunkCount();++i)result+=catalogChunk(i);
  return result;
}

const Catalog& bundledCatalog(){
  static const Catalog catalog=parseCatalog(Json::parse(bundledCatalogText()));
  return catalog;
}

std::string catalogSection(const CatalogProvider& provider){
  if(!provider.supported())return "Unsupported yet";
  if(provider.local)return "Local servers";
  return "Cloud";
}

std::string kindLabel(const std::string& kind){
  if(kind=="anthropic")return "Anthropic Messages";
  if(kind=="openai-chat")return "OpenAI Chat (translated)";
  if(kind=="bedrock")return "AWS Bedrock";
  if(kind=="vertex")return "Google Vertex";
  return "unsupported";
}

std::string shortKindLabel(const std::string& kind){
  if(kind=="anthropic")return "Anthropic API";
  if(kind=="openai-chat")return "OpenAI chat";
  if(kind=="bedrock")return "Bedrock";
  if(kind=="vertex")return "Vertex";
  return "unsupported";
}

std::string compactTokens(std::size_t tokens){
  // Limits are often powers of two: 65536 reads as 64K, 1048576 as 1M.
  if(tokens>=1048576&&tokens%1048576==0)return std::to_string(tokens/1048576)+"M";
  if(tokens>=1000000&&tokens%1000000==0)return std::to_string(tokens/1000000)+"M";
  if(tokens>=1000&&tokens<1000000&&tokens%1000!=0&&tokens%1024==0)
    return std::to_string(tokens/1024)+"K";
  if(tokens>=1000000){
    const auto tenths=(tokens+50000)/100000;
    return std::to_string(tenths/10)+(tenths%10?"."+std::to_string(tenths%10):"")+"M";
  }
  if(tokens>=1000)return std::to_string((tokens+500)/1000)+"K";
  return std::to_string(tokens);
}

std::string describeModel(const CatalogModel& model){
  std::string result;
  const auto add=[&](const std::string& part){
    if(!result.empty())result+=" · ";
    result+=part;
  };
  if(model.context)add(compactTokens(model.context)+" context");
  if(model.output)add(compactTokens(model.output)+" output");
  if(model.toolCall)add("tools");
  if(model.reasoning)add("reasoning");
  if(std::find(model.inputs.begin(),model.inputs.end(),"image")!=model.inputs.end())add("images");
  return result;
}
} // namespace batbox::providers
