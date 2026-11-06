#include "providers/LocalDiscovery.hpp"
#include "transport/RawHttp.hpp"
#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <thread>

namespace batbox::providers {
namespace {
struct Reply {
  bool reached=false;
  int status=0;
  Json body;
  std::string server;
};
Reply get(const mcp::http::Fetch& fetch,const std::string& url,std::chrono::milliseconds timeout){
  Reply reply;
  try{
    mcp::http::Request request;
    request.url=url;request.method="GET";request.followRedirects=false;
    request.timeout=timeout;request.connectTimeout=timeout;
    request.headers["accept"]="application/json";
    const auto response=fetch(request);
    reply.reached=true;reply.status=response.status;
    reply.server=response.header("server");
    if(response.ok())try{reply.body=response.json();}catch(...){reply.body=Json();}
  }catch(...){}
  return reply;
}
std::string lowered(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
void addModel(LocalServer& server,std::set<std::string>& seen,const Json& item,
    std::initializer_list<const char*> keys){
  if(item.is_string()){
    const auto id=item.get<std::string>();
    if(!id.empty()&&seen.insert(id).second)server.models.push_back(id);
    return;
  }
  if(!item.is_object())return;
  for(const char* key:keys){
    const auto found=item.find(key);
    if(found==item.end()||!found->is_string())continue;
    const auto id=found->get<std::string>();
    if(id.empty()||id.find_first_of("\r\n")!=std::string::npos)return;
    if(seen.insert(id).second)server.models.push_back(id);
    if(const auto context=transport::reportedContextWindow(item))server.contexts[id]=context;
    return;
  }
}
// {"data":[{"id":...}]} or {"models":[...]}; false when neither list exists.
bool readModels(LocalServer& server,const Json& body,std::initializer_list<const char*> keys){
  if(!body.is_object())return false;
  std::set<std::string> seen(server.models.begin(),server.models.end());
  bool found=false;
  for(const char* list:{"data","models"}){
    const auto rows=body.find(list);
    if(rows==body.end()||!rows->is_array())continue;
    found=true;
    for(const auto& item:*rows){
      addModel(server,seen,item,keys);
      if(server.models.size()>=500)break;
    }
  }
  return found;
}
bool ownedByVllm(const Json& body){
  if(!body.is_object()||!body.contains("data")||!body.at("data").is_array())return false;
  for(const auto& item:body.at("data"))
    if(item.is_object()&&item.value("owned_by","")=="vllm")return true;
  return false;
}
bool looksLikeLlamaProps(const Reply& props){
  if(lowered(props.server).find("llama")!=std::string::npos)return true;
  if(!props.body.is_object())return false;
  for(const char* key:{"build_info","model_path","chat_template","default_generation_settings",
      "total_slots"})
    if(props.body.contains(key))return true;
  return false;
}
std::optional<LocalServer> probe(const ProbeTarget& target,std::chrono::milliseconds timeout,
    const mcp::http::Fetch& fetch){
  LocalServer server;
  server.preset=target.preset;server.name=target.name;server.port=target.port;
  server.baseUrl="http://"+target.host+":"+std::to_string(target.port);
  const auto& base=server.baseUrl;
  if(target.preset=="ollama"){
    const auto tags=get(fetch,base+"/api/tags",timeout);
    if(!tags.reached||!readModels(server,tags.body,{"name","model"}))return std::nullopt;
    return server;
  }
  if(target.preset=="lmstudio"){
    const auto rest=get(fetch,base+"/api/v1/models",timeout);
    if(rest.reached&&readModels(server,rest.body,{"key","id"}))return server;
    const auto models=get(fetch,base+"/v1/models",timeout);
    if(!models.reached||!readModels(server,models.body,{"id"}))return std::nullopt;
    return server;
  }
  const auto models=get(fetch,base+"/v1/models",timeout);
  if(!models.reached||!readModels(server,models.body,{"id","model","name"}))return std::nullopt;
  if(ownedByVllm(models.body)){
    server.preset="vllm";server.name="vLLM";
    return server;
  }
  const auto props=get(fetch,base+"/props",timeout);
  if(props.reached&&looksLikeLlamaProps(props)){
    server.preset="llama.cpp";server.name="llama.cpp";
    if(props.body.is_object()&&props.body.contains("default_generation_settings")){
      const auto& settings=props.body.at("default_generation_settings");
      if(settings.is_object()&&settings.contains("n_ctx")&&settings.at("n_ctx").is_number_unsigned())
        for(const auto& id:server.models)server.contexts[id]=settings.at("n_ctx").get<std::size_t>();
    }
    return server;
  }
  server.confirmed=target.preset!="llama.cpp";
  return server;
}
}

std::vector<ProbeTarget> defaultProbeTargets(){
  return {{"ollama","Ollama","127.0.0.1",11434},{"lmstudio","LM Studio","127.0.0.1",1234},
    {"llama.cpp","llama.cpp","127.0.0.1",9931},{"llama.cpp","llama.cpp","127.0.0.1",8080},
    {"vllm","vLLM","127.0.0.1",8000}};
}

std::vector<LocalServer> discoverLocalServers(const std::vector<ProbeTarget>& targets,
    std::chrono::milliseconds timeout,mcp::http::Fetch fetch){
  std::vector<std::optional<LocalServer>> found(targets.size());
  {
    std::vector<std::jthread> workers;
    workers.reserve(targets.size());
    for(std::size_t i=0;i<targets.size();++i)
      workers.emplace_back([&found,&targets,&fetch,timeout,i]{
        try{found[i]=probe(targets[i],timeout,fetch);}catch(...){}
      });
  }
  std::vector<LocalServer> result;
  for(auto& server:found)if(server)result.push_back(std::move(*server));
  return result;
}

std::string describeLocalServer(const LocalServer& server){
  std::string text=server.name;
  if(server.preset=="llama.cpp"||!server.confirmed)text+=" on :"+std::to_string(server.port);
  if(!server.confirmed)text+=" (unidentified)";
  const auto count=server.models.size();
  text+=" with "+std::to_string(count)+(count==1?" model":" models");
  return text;
}
} // namespace batbox::providers
