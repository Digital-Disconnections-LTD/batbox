#include "interactive/message_list_view/Search.hpp"
#include <iostream>
using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::message_list_view;
int main(){
  Json cases;std::cin>>cases;Json results=Json::array();
  for(const auto& spec:cases){
    const auto action=spec.at("action").get<std::string>();const auto& value=spec.at("value");
    if(action=="use")results.push_back(gadgetUseSearchText(value));
    else if(action=="result")results.push_back(gadgetResultSearchText(value));
    else if(action=="raw")results.push_back(computeSearchText(value));
    else if(action=="render"){
      SearchTextStash stash;results.push_back(stash.renderable(std::make_shared<Json>(value)));
    }else{
      auto message=std::make_shared<Json>(value);SearchTextStash stash;MessageLookups lookups;
      auto owner=std::make_shared<Json>(Json{{"message",{{"content",Json::array({{{"name","Read"}}})}}}});
      lookups.gadgetUseByGadgetUseID["t"]={owner,0};int calls=0;
      gadgets::Gadget gadget;gadget.name="Read";
      gadget.extractSearchText=[&](const Json&){++calls;return spec.at("extracted").get<std::string>();};
      gadgets::Gadgets roster;if(spec.value("tool",false))roster.push_back(&gadget);
      const auto first=stash.extract(message,lookups,roster);
      (*message)["toolUseResult"]={{"stdout","MUTATED"}};
      const auto second=stash.extract(message,lookups,roster);
      results.push_back({{"first",first},{"second",second},{"calls",calls}});
    }
  }
  std::cout<<results.dump()<<'\n';
}
