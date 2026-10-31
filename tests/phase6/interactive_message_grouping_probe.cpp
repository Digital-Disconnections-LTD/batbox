#include "interactive/MessageGrouping.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
int main(int argc,char** argv) {
  if(argc!=3||std::string(argv[1])!="--cases")return 2;
  std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json out=Json::array();
  for(const auto& spec:cases){
    ix::MessageRefs messages;
    for(const auto& m:spec.at("messages"))messages.push_back(std::make_shared<Json>(m));
    auto items=ix::asRenderItems(messages);
    auto gadgets=std::make_shared<ix::GroupingGadgetSet>();
    for(const auto& t:spec.at("tools"))gadgets->gadgets.push_back({t.at("name"),t.at("renderGroupedToolUse")});
    auto grouped=ix::applyGrouping(items,gadgets,spec.value("verbose",false));
    Json rows=Json::array();bool identity=true;
    for(const auto& item:*grouped){
      const auto value=ix::materializeRenderItem(item);
      Json members=nullptr,results=nullptr,display=nullptr;
      if(!item.members.empty()){
        members=Json::array();results=Json::array();
        for(const auto& m:item.members){members.push_back(m->at("uuid"));
          if(std::find(messages.begin(),messages.end(),m)==messages.end())identity=false;}
        for(const auto& m:item.results){results.push_back(m->at("uuid"));
          if(std::find(messages.begin(),messages.end(),m)==messages.end())identity=false;}
        display=item.displayMessage->at("uuid");
        if(item.displayMessage!=item.members.front())identity=false;
      }
      rows.push_back(Json{{"type",value.at("type")},{"uuid",value.at("uuid")},
        {"messages",members},{"results",results},{"display",display}});
    }
    out.push_back(Json{{"name",spec.at("name")},{"same",grouped==items},
      {"items",rows},{"groupIdentity",identity}});
  }
  std::cout<<out.dump()<<'\n';
}
