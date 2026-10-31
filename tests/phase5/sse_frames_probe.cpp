#include "cli/remote/SSEFrames.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line).get<std::string>();
    const auto parsed=cli::remote::parseSSEFrames(input);
    Json frames=Json::array();
    for(const auto& frame:parsed.frames){
      Json value=Json::object();
      if(frame.event)value["event"]=*frame.event;
      if(frame.id)value["id"]=*frame.id;
      if(frame.data)value["data"]=*frame.data;
      frames.push_back(std::move(value));
    }
    std::cout<<value::stringify(Json{{"frames",frames},
      {"remaining",parsed.remaining}})<<'\n';
  }
}
