#include "interactive/ReadSearchCollapse.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
int main(int argc,char**argv){if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json rows=Json::array();
 for(const auto&spec:cases){
  ix::MessageRefs originals;
  for(const auto&m:spec.at("messages"))originals.push_back(std::make_shared<Json>(m));
  std::vector<batbox::gadgets::Gadget> owned;
  auto grouping=std::make_shared<ix::GroupingGadgetSet>();
  for(const auto&t:spec.at("tools")){
   batbox::gadgets::Gadget gadget;gadget.name=t.at("name");
   if(t.contains("result")&&!t.at("result").is_null())
    gadget.isSearchOrReadCommand=[value=t.at("result")](const Json&){return value;};
   owned.push_back(std::move(gadget));
   grouping->gadgets.push_back({t.at("name"),t.value("renderGroupedToolUse",false)});
  }
  ix::ReadSearchImports imports;
  for(auto&gadget:owned)imports.gadgets.push_back(&gadget);
  const auto features=spec.value("features",Json::object());
  const bool fullscreen=spec.value("fullscreen",false);
  auto&c=imports.classification;
  c.feature=[features](const std::string&name){return features.value(name,false);};
  c.isFullscreen=[fullscreen]{return fullscreen;};
  c.isAutoManagedMemoryFile=[](const std::string&p){return p=="MEMORY.md";};
  c.isAutoManagedMemoryPattern=[](const std::string&p){return p.find("MEMORY")!=std::string::npos;};
  c.isMemoryDirectory=[](const std::string&p){return p=="memory";};
  c.isShellCommandTargetingMemory=[](const std::string&p){return p.find("MEMORY")!=std::string::npos;};
  c.getReplPrimitiveGadgets=[]{return batbox::gadgets::Gadgets{};};
  c.replGadgetName="REPL";c.writeGadgetName="Write";c.editGadgetName="Edit";
  c.gadgetSearchName="ToolSearch";c.bashGadgetName="Bash";
  c.snipGadgetName=features.value("HISTORY_SNIP",false)?std::optional<std::string>{"Snip"}:std::nullopt;
  imports.getDisplayPath=[](const std::string&p){return p;};
  imports.extractBashCommentLabel=[](const std::string&)->std::optional<std::string>{return {};};
  imports.isTeamMemoryWriteOrEdit=[](const std::string&,const Json&v){return v.value("file_path","")=="TEAMMEM.md";};
  imports.isTeamMemorySearch=[](const Json&v){return v.value("path","")=="TEAMMEM.md";};
  imports.isTeamMemFile=[](const std::string&p){return p=="TEAMMEM.md";};
  imports.detectGitOperation=[](const std::string&command,const std::string&){return Json{{"commit",command.starts_with("git commit")?Json{{"sha","abc"},{"kind","commit"}}:Json(nullptr)},{"push",nullptr},{"branch",nullptr},{"pr",nullptr}};};
  auto grouped=ix::applyGrouping(ix::asRenderItems(originals),grouping,false);
  auto output=ix::collapseReadSearchGroups(grouped,imports);
  Json values=Json::array(),retained=Json::array();bool identity=true;
  for(const auto&item:*output){values.push_back(ix::materializeRenderItem(item));
   const auto found=std::find(originals.begin(),originals.end(),item.message);
   retained.push_back(found==originals.end()?-1:static_cast<int>(found-originals.begin()));
   if(item.collapsedItems&&(!item.displayItem||
      item.displayItem->message!=item.collapsedItems->front().message))identity=false;
  }
  rows.push_back(Json{{"name",spec.at("name")},{"output",values},
    {"retained",retained},{"groupIdentity",identity}});
 }
 std::cout<<rows.dump()<<'\n';
}
