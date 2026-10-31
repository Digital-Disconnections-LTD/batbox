#include "application_bats/BatMcp.hpp"
#include "application/Async.hpp"
#include <algorithm>
#include <deque>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
struct BatServer {
  mcp::manager::ServerResult result;
  bool inlineServer=false;
};
struct BatConnections {
  std::deque<BatServer> servers;
  void closeInline(){
    for(auto& record:servers)
      if(record.inlineServer&&record.result.type=="connected"&&
          record.result.close)record.result.close();
  }
  ~BatConnections(){
    try{closeInline();}catch(...){}
  }
};
Json clientRow(const mcp::manager::ServerResult& server){
  Json row={{"name",server.name},{"type",server.type},{"config",server.config}};
  if(!server.error.empty())row["error"]=server.error;
  if(server.connection){
    row["capabilities"]=server.connection->capabilities;
    row["serverInfo"]=server.connection->serverInfo;
    if(!server.connection->instructions.empty())
      row["instructions"]=server.connection->instructions;
  }
  return row;
}
void note(const std::function<void(const std::string&)>& diagnostic,
    const std::string& message){if(diagnostic)diagnostic(message);}
}
bats::BatMcpSetup initializeBatMcpServers(const Json& definition,
    const Json& parentClients,
    const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
    event_loop::Loop::Dispatcher owner,
    const std::function<void(const std::string&)>& diagnostic){
  bats::BatMcpSetup output;
  output.clients=parentClients.is_array()?parentClients:Json::array();
  const auto& specifications=field(definition,"mcpServers");
  if(specifications.is_null()||
      (specifications.is_array()&&specifications.empty())){
    output.cleanup=[]{ };return output;
  }
  if(!specifications.is_array())
    throw std::invalid_argument("Agent MCP servers must be an array");
  if(!runtime)throw std::runtime_error(
    "Agent-specific MCP servers need the native MCP runtime");
  const auto configurations=application::onOwnerAndWait<Json>(owner,
    [runtime]{return runtime->configurations();});
  auto connections=std::make_shared<BatConnections>();
  const auto& batType=field(definition,"agentType");
  const std::string label=batType.is_string()?batType.get<std::string>():"agent";
  for(const auto& spec:specifications){
    std::string name;Json config;bool inlineServer=false;
    if(spec.is_string()){
      name=spec.get<std::string>();
      if(!configurations.is_object()||!configurations.contains(name)){
        note(diagnostic,"Agent "+label+" MCP server not found: "+name);continue;
      }
      config=configurations.at(name);
    }else if(spec.is_object()){
      if(spec.size()!=1){note(diagnostic,"Agent "+label+
        " MCP server spec must have exactly one name");continue;}
      auto it=spec.begin();name=it.key();config=it.value();inlineServer=true;
      if(!config.is_object()){note(diagnostic,"Agent "+label+
        " MCP server configuration must be an object: "+name);continue;}
      config["scope"]="dynamic";
    }else{note(diagnostic,"Agent "+label+" has an invalid MCP server spec");continue;}
    mcp::manager::ServerResult connected;
    if(inlineServer){
      connected=application::awaitOnOwner<mcp::manager::ServerResult>(owner,
        [runtime,name,config]{return runtime->connectBatServer(name,config);});
    }else{
      const auto snapshot=application::onOwnerAndWait<
        std::vector<mcp::manager::ServerResult>>(owner,[runtime]{
          return runtime->manager()->snapshot();});
      auto found=std::find_if(snapshot.begin(),snapshot.end(),
        [&name](const auto& server){return server.name==name;});
      if(found!=snapshot.end()&&found->type=="connected")connected=*found;
      else if(found!=snapshot.end()&&found->type=="disabled"){
        note(diagnostic,"Agent "+label+" MCP server is disabled: "+name);continue;
      }else connected=application::awaitOnOwner<mcp::manager::ServerResult>(owner,
        [runtime,name]{return runtime->manager()->reconnect(name);});
    }
    output.clients.push_back(clientRow(connected));
    connections->servers.push_back({std::move(connected),inlineServer});
    auto& server=connections->servers.back().result;
    if(server.type=="connected"){
      for(auto& gadget:server.gadgets)output.gadgets.push_back(&gadget);
      note(diagnostic,"Agent "+label+" connected to MCP server "+name+
        " with "+std::to_string(server.gadgets.size())+" tools");
    }else note(diagnostic,"Agent "+label+" MCP server "+name+
      " did not connect: "+server.type);
  }
  output.cleanup=[connections,diagnostic]{
    for(auto& record:connections->servers){
      if(!record.inlineServer||record.result.type!="connected"||
          !record.result.close)continue;
      try{record.result.close();}
      catch(const std::exception& error){note(diagnostic,
        "Agent MCP cleanup failed for "+record.result.name+": "+error.what());}
    }
  };
  return output;
}
} // namespace batbox::application_bats
