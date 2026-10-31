#include "text_width/Tabs.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){using namespace batbox;Json items;std::cin>>items;Json all=Json::array();for(const auto& item:items){Json calls=Json::array(),value;
 const auto width=[&](const std::string& text)->double{calls.push_back(text);return item.value("nan",false)?NAN:static_cast<double>(value::stringLength(text));};
 try{const auto text=item.at("text").get<std::string>();if(item.value("widest",false))value=text_width::widestLine(text,width);
 else value=text_width::expandTabs(text,item.value("interval",8.0),width);}catch(const std::exception& e){value=Json{{"error",e.what()}};}
 all.push_back(Json{{"value",value},{"calls",calls}});}std::cout<<value::stringify(all)<<'\n';}
