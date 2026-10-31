#include "execution/Orchestration.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const Json item=value::parse(line);
  std::vector<gadgets::Gadget> gadgetsOwned;
  for(const Json& source:item.at("tools")){
    gadgets::Gadget gadget;gadget.name=source.at("name").get<std::string>();
    const bool parseThrows=source.value("parseThrows",false);
    const bool safeThrows=source.value("safeThrows",false);
    const bool safe=source.value("safe",false);
    gadget.parseInput=[parseThrows](const Json& input)->Json{
      if(parseThrows)throw std::runtime_error("parse");
      return input;
    };
    gadget.isConcurrencySafe=[safeThrows,safe](const Json& input){
      if(safeThrows)throw std::runtime_error("safe");
      return safe&&value::property(input,"allow")!=false;
    };
    gadgetsOwned.push_back(std::move(gadget));
  }
  execution::Context context;
  for(auto& gadget:gadgetsOwned)context.gadgets.push_back(&gadget);
  std::vector<Json> blocks;
  for(const Json& block:item.at("blocks"))blocks.push_back(block);
  Json batches=Json::array();
  for(const auto& batch:execution::partitionGadgetCalls(blocks,context)){
    Json ids=Json::array();for(const Json& block:batch.blocks)ids.push_back(block.at("id"));
    batches.push_back(Json{{"isConcurrencySafe",batch.isConcurrencySafe},{"ids",ids}});
  }
  std::cout<<value::stringify(Json{{"cap",execution::getMaxGadgetUseConcurrency(item.value("cap",std::string{}))},
    {"batches",batches}})<<'\n';
}catch(const std::exception& error){std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}}
