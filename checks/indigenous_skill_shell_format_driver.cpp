#include "application_skills/ShellExpansion.hpp"
#include "skills/process/PoetryCommand.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
std::string field(const batbox::Json& item,const char* key){
  const auto& value=batbox::value::property(item,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
}
int main(){
  using batbox::Json;
  std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=Json::parse(input);
  Json output=Json::array();
  for(const auto& item:cases){
    try{
      const auto text=batbox::application_skills::expandPoetryShell(
        item.at("text").get<std::string>(),"fixture",
        [&item](const std::string&,const std::string& pattern){
          const auto& deny=batbox::value::property(item,"deny");
          if(batbox::value::truthy(deny)){
            const auto reason=deny.is_string()?deny.get<std::string>():"";
            throw batbox::skills::process::MalformedCommandError(
              "Shell command permission check failed for pattern \""+pattern+
              "\": "+(reason.empty()?"Permission denied":reason));
          }
          const auto shellError=field(item,"shellError");
          if(!shellError.empty())throw batbox::bash_gadget::ShellError(
            field(item,"stdout"),field(item,"stderr"),1,
            shellError=="interrupted");
          const auto error=field(item,"error");
          if(!error.empty())throw std::runtime_error(error);
          const auto& mapped=batbox::value::property(item,"mapped");
          if(mapped.is_string())return mapped.get<std::string>();
          return batbox::application_skills::formatPoetryShellOutput(
            field(item,"stdout"),field(item,"stderr"));
        });
      output.push_back({{"text",text}});
    }catch(const std::exception& error){
      output.push_back({{"error",error.what()}});
    }
  }
  std::cout<<output.dump()<<'\n';
}
