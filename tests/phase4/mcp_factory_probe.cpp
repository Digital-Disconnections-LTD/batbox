#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    auto connection=std::make_shared<batbox::mcp::Connection>();
    connection->connected=true;connection->hasGadgetCapability=true;
    connection->name=input.value("server",std::string("demo"));
    connection->skipPrefix=input.value("skip",false);
    connection->listGadgets=[input](std::stop_token){return batbox::Json{{"tools",input.at("tools")}};};
    connection->callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
        const std::function<void(const batbox::Json&)>&){return batbox::mcp::GadgetCallResult{};};
    connection->ensureConnected=[](){};
    batbox::mcp::Services services;
    services.wire.searchHintMetaKey="anthropic/searchHint";
    services.wire.alwaysLoadMetaKey="anthropic/alwaysLoad";
    services.wire.hostedServerPrefix="claude.ai ";
    services.nowMs=[](){return std::int64_t{1};};
    services.processResult=[](const batbox::Json&,const std::string&,const std::string&){
      return batbox::Json("done");};
    services.isOutputLineTruncated=[](const std::string&){return false;};
    services.renderGadgetUseMessage=[](const batbox::Json&,const batbox::Json&){return batbox::Json();};
    services.renderGadgetResultMessage=[](const batbox::Json&,const batbox::Json&,
      const batbox::Json&){return batbox::Json();};
    services.renderGadgetUseProgressMessage=[](const batbox::Json&,const batbox::Json&){
      return batbox::Json();};
    batbox::Json result=batbox::Json::array();
    for(const auto& gadget:batbox::mcp::fetchGadgetsForClient(connection,services)){
      const auto value=input.value("input",batbox::Json::object());
      result.push_back(batbox::Json{{"name",gadget.name},{"mcpInfo",gadget.mcpInfo},
        {"isMcp",gadget.isMcp.value_or(false)},
        {"searchHint",gadget.searchHint?batbox::Json(*gadget.searchHint):batbox::Json()},
        {"alwaysLoad",gadget.alwaysLoad.value_or(false)},
        {"description",gadget.description({},{}).get()},{"prompt",gadget.poetry({}).get()},
        {"readOnly",gadget.isReadOnly({})},{"concurrencySafe",gadget.isConcurrencySafe({})},
        {"destructive",gadget.isDestructive({})},{"openWorld",gadget.isOpenWorld({})},
        {"inputJSONSchema",gadget.inputJSONSchema},
        {"permission",gadget.checkPermissions({},{}).get()},
        {"userFacingName",gadget.userFacingName({})},
        {"classifier",gadget.toAutoClassifierInput(value)},
        {"searchOrRead",gadget.isSearchOrReadCommand(value)},
        {"resultBlock",gadget.mapGadgetResultToGadgetResultBlockParam("result","id")}});
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
