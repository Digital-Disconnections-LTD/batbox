#include "interactive/GadgetClassification.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
namespace {
Json json(const ix::SearchOrReadResult& r,bool fromContent){
 Json out{{"isSearch",r.isSearch},{"isRead",r.isRead},{"isList",r.isList},
  {"isREPL",r.isREPL},{"isMemoryWrite",r.isMemoryWrite},
  {"isAbsorbedSilently",r.isAbsorbedSilently}};
 if(!fromContent)out["isCollapsible"]=r.isCollapsible;
 if(r.mcpServerName)out["mcpServerName"]=*r.mcpServerName;
 if(r.isBash)out["isBash"]=*r.isBash;
 return out;
}
} // namespace
int main(int argc,char**argv){if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json rows=Json::array();
 for(const auto&c:cases){
  std::vector<batbox::gadgets::Gadget> owned,primitiveOwned;
  batbox::gadgets::Gadgets gadgets,primitive;
  for(const auto&t:c.at("tools")){
   batbox::gadgets::Gadget gadget;gadget.name=t.at("name");
   if(t.contains("aliases"))gadget.aliases=t.at("aliases").get<std::vector<std::string>>();
   if(t.contains("result"))gadget.isSearchOrReadCommand=[result=t.at("result")](const Json&){return result;};
   if(t.contains("isMcp"))gadget.isMcp=t.at("isMcp").get<bool>();
   if(t.contains("mcpInfo"))gadget.mcpInfo=t.at("mcpInfo");
   owned.push_back(std::move(gadget));
  }
  for(auto&gadget:owned)gadgets.push_back(&gadget);
  if(c.contains("primitive"))for(const auto&t:c.at("primitive")){
   batbox::gadgets::Gadget gadget;gadget.name=t.at("name");
   gadget.isSearchOrReadCommand=[result=t.at("result")](const Json&){return result;};
   primitiveOwned.push_back(std::move(gadget));
  }
  for(auto&gadget:primitiveOwned)primitive.push_back(&gadget);
  const auto features=c.value("features",Json::object());
  ix::GadgetClassificationImports imports;
  imports.feature=[features](const std::string&name){return features.value(name,false);};
  imports.isFullscreen=[on=c.value("fullscreen",false)]{return on;};
  imports.isAutoManagedMemoryFile=[](const std::string&p){return p=="MEMORY.md";};
  imports.isMemoryDirectory=[](const std::string&p){return p=="memory";};
  imports.isAutoManagedMemoryPattern=[](const std::string&p){return p.find("MEMORY")!=std::string::npos;};
  imports.isShellCommandTargetingMemory=[](const std::string&p){return p.find("MEMORY")!=std::string::npos;};
  imports.getReplPrimitiveGadgets=[primitive]{return primitive;};
  imports.replGadgetName="REPL";imports.writeGadgetName="Write";imports.editGadgetName="Edit";
  imports.gadgetSearchName="ToolSearch";imports.snipGadgetName="Snip";imports.bashGadgetName="Bash";
  const auto info=ix::getGadgetSearchOrReadInfo(c.at("toolName"),c.at("input"),gadgets,imports);
  const Json content{{"type","tool_use"},{"name",c.at("toolName")},{"input",c.at("input")}};
  const auto nested=ix::getSearchOrReadFromContent(&content,gadgets,imports);
  rows.push_back(Json{{"name",c.at("name")},{"info",json(info,false)},
   {"fromContent",nested?json(*nested,true):Json(nullptr)}});
 }
 std::cout<<rows.dump()<<'\n';
}
