#include "user_input/TextPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  batbox::messages::Services services;
  services.randomUUID=[]{return "id";};
  services.nowISO=[]{return "now";};
  while(std::getline(std::cin,line)){
    try{
      const auto test=batbox::value::parse(line);
      auto optional=[](const batbox::Json& value)->std::optional<std::string>{
        return value.is_string()?std::optional<std::string>(value.get<std::string>()):
          std::nullopt;
      };
      const auto result=batbox::user_input::processTextPoetry(test.at("input"),
        test.at("images"),test.at("ids"),test.at("attachments"),
        optional(batbox::value::property(test,"uuid")),
        optional(batbox::value::property(test,"permissionMode")),
        batbox::value::property(test,"isMeta")==true,services);
      batbox::Json messages=batbox::Json::array();
      for(const auto& message:result.messages){
        if(batbox::value::property(message,"type")=="user"){
          batbox::Json projected={{"type","user"},
            {"content",message.at("message").at("content")}};
          for(const auto* key:{"uuid","permissionMode","isMeta","imagePasteIds"})
            if(message.contains(key)&&
               (std::string(key)!="uuid"||test.contains("uuid")))
              projected[key]=message.at(key);
          messages.push_back(std::move(projected));
        }else messages.push_back(message);
      }
      std::cout<<batbox::value::stringify(batbox::Json{{"messages",messages},
        {"shouldQuery",result.shouldQuery}})<<'\n';
    }catch(const std::exception& e){
      std::cout<<batbox::value::stringify(batbox::Json{{"error",e.what()}})<<'\n';
    }
  }
}
