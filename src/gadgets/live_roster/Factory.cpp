#include "gadgets/live_roster/LiveRoster.hpp"
#include <stdexcept>

namespace batbox::gadgets::live_roster {
namespace {
void require(bool ready,const char* dependency){
  if(!ready)throw std::invalid_argument(std::string("Live tool import required: ")+
    dependency);
}
void add(Snapshot& snapshot,Gadget gadget,bool mcpGadget=false){
  require(!gadget.name.empty(),"tool name");
  require(bool(gadget.call),"tool call");
  require(bool(gadget.parseInput),"tool input parser");
  require(!gadget.inputJSONSchema.is_null(),"tool input schema");
  require(bool(gadget.description),"tool description");
  require(bool(gadget.poetry),"tool prompt");
  require(bool(gadget.checkPermissions),"tool permission method");
  require(bool(gadget.mapGadgetResultToGadgetResultBlockParam),"tool result mapper");
  require(bool(gadget.renderGadgetUseMessage),"tool use renderer");
  require(bool(gadget.renderGadgetResultMessage),"tool result renderer");
  require(bool(gadget.isEnabled),"tool enabled method");
  snapshot.owned.push_back(std::move(gadget));
  const Gadget* owned=&snapshot.owned.back();
  (mcpGadget?snapshot.mcp:snapshot.base).push_back(owned);
}
Gadget batGadget(const BatFactory& factory){
  require(bool(factory.makeServices),"Agent runner services");
  Gadget gadget;
  bats::configureBatGadgetDescriptor(gadget,factory.descriptor);
  bats::configureBatGadgetInputSchema(gadget,factory.schema);
  bats::bindBatGadgetCall(gadget,factory.makeContext,
    factory.makeServices);
  bats::bindBatGadgetPoetry(gadget,factory.poetry);
  return buildGadget(std::move(gadget));
}
void requireSharedStash(const FactoryInputs& input){
  const auto& read=input.read->readFileStateOwner;
  const auto& write=input.write->readFileStateOwner;
  const auto& edit=input.edit->readFileStateOwner;
  require(bool(read)&&read==write&&read==edit,
    "one shared bounded read-file state cache");
}
}

std::shared_ptr<Snapshot> makeSnapshot(FactoryInputs input){
  require(input.bat.has_value()||bool(input.makeBatGadget),"Agent factory");
  require(input.bash.has_value(),"Bash services");
  require(input.read.has_value(),"Read services");
  require(input.write.has_value(),"Write services");
  require(input.edit.has_value(),"Edit services");
  require(input.glob.has_value(),"Glob services");
  require(input.grep.has_value(),"Grep services");
  require(input.search.has_value()&&bool(input.searxng),"SearXNG search");
  require(input.fetch.has_value()&&bool(input.scrapling),"Scrapling fetch");
  require(bool(input.fetch->querySmallModel),
    "WebFetch prompt-model binding");
  requireSharedStash(input);
  input.read->readFileState=input.read->readFileStateOwner.get();
  input.write->readFileState=input.write->readFileStateOwner.get();
  input.edit->readFileState=input.edit->readFileStateOwner.get();
  auto result=std::make_shared<Snapshot>();
  result->searxng=std::move(input.searxng);
  result->scrapling=std::move(input.scrapling);
  result->mcpConnections=std::move(input.mcpConnections);
  add(*result,input.makeBatGadget?input.makeBatGadget():batGadget(*input.bat));
  add(*result,input.makeBash?input.makeBash(std::move(*input.bash)):
    bash_gadget::makeBashGadget(std::move(*input.bash)));
  const auto fileBindings=[&](const std::string& name){
    return input.makeFileBindings?input.makeFileBindings(name):input.fileBindings;};
  add(*result,files::makeGlobGadget(std::move(*input.glob),fileBindings("Glob")));
  add(*result,files::makeGrepGadget(std::move(*input.grep),fileBindings("Grep")));
  add(*result,files::makeReadGadget(std::move(*input.read),fileBindings("Read")));
  add(*result,files::makeEditGadget(std::move(*input.edit),fileBindings("Edit")));
  add(*result,files::makeWriteGadget(std::move(*input.write),fileBindings("Write")));
  input.fetch->connector=result->scrapling.get();
  add(*result,web::makeWebFetchGadget(std::move(*input.fetch)));
  input.search->connector=result->searxng.get();
  add(*result,web::makeWebSearchGadget(std::move(*input.search)));
  for(auto& gadget:input.additionalBaseGadgets)add(*result,std::move(gadget));
  for(const auto& connection:result->mcpConnections){
    auto discovered=mcp::fetchGadgetsForClient(connection,input.mcpServices);
    for(auto& gadget:discovered)add(*result,std::move(gadget),true);
  }
  return result;
}

const Gadget* Snapshot::find(const std::string& name)const{
  if(const auto* gadget=findGadgetByName(base,name))return gadget;
  return findGadgetByName(mcp,name);
}
} // namespace batbox::gadgets::live_roster
