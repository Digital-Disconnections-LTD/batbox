#include "termio/Tokenizer.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){using namespace batbox;Json items;std::cin>>items;Json all=Json::array();
 for(const auto& item:items){termio::Tokenizer t(item.value("x10Mouse",false));Json results=Json::array();
  for(const auto& step:item.at("steps")){Json value=nullptr;std::vector<termio::Token> tokens;
   const auto op=step.at("op").get<std::string>();
   if(op=="feed")tokens=t.feed(step.at("text").get<std::string>());
   else if(op=="flush")tokens=t.flush();else if(op=="reset")t.reset();
   if(op=="feed"||op=="flush"){value=Json::array();for(const auto& token:tokens)value.push_back(Json{{"type",token.type},{"value",token.value}});}
   results.push_back(Json{{"value",value},{"buffer",t.buffer()}});
  }all.push_back(results);
 }std::cout<<value::stringify(all)<<'\n';
}
