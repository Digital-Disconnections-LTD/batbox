#include "interactive/context/GadgetPool.hpp"
#include <iostream>
#include <unordered_map>

using namespace batbox;
namespace ctx=batbox::interactive::context;
int main(){
  std::vector<gadgets::Gadget> owned(7);
  const std::vector<std::string> names={"Beta","mcp__z","Agent","Alpha",
    "Beta","mcp__a","mcp__subscribe_pr_activity"};
  for(std::size_t i=0;i<owned.size();++i)owned[i].name=names[i];
  gadgets::Gadgets initial={&owned[0],&owned[1],&owned[2]};
  gadgets::Gadgets assembled={&owned[3],&owned[4],&owned[5],&owned[6]};
  for(bool coordinator:{false,true}){
    ctx::GadgetPoolServices services;
    services.localeCompare=[](const auto& a,const auto& b){return a.compare(b);};
    services.feature=[coordinator](const auto&){return coordinator;};
    services.isCoordinatorMode=[coordinator]{return coordinator;};
    services.coordinatorAllowedNames={"Agent","TaskStop","SendMessage",
      "SyntheticOutput"};
    const auto merged=ctx::mergeAndFilterGadgets(initial,assembled,"default",services);
    Json output=Json::array();
    for(const auto* gadget:merged)
      output.push_back(Json::array({gadget->name,
        gadget==&owned[0]||gadget==&owned[1]||gadget==&owned[2]?
          "initial":"assembled"}));
    std::cout<<Json{{"coordinator",coordinator},{"tools",output}}.dump()<<'\n';
  }
  const Json clients=ctx::mergeClients(
    Json::array({Json{{"name","one"},{"tag","initial"}},
      Json{{"name","two"},{"tag","initial"}}}),
    Json::array({Json{{"name","two"},{"tag","connected"}},
      Json{{"name","three"},{"tag","connected"}}}));
  std::cout<<Json{{"clients",clients}}.dump()<<'\n';
  std::cout<<Json{{"clients",ctx::mergeClients(Json(),
    Json::array({Json{{"name","one"}}}))}}.dump()<<'\n';
  std::cout<<Json{{"clients",ctx::mergeClients(
    Json::array({Json{{"name","undefined"},{"tag","named"}}}),
    Json::array({Json{{"tag","missing"}}}))}}.dump()<<'\n';
}
