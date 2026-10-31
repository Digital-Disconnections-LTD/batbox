#include "gadgets/cron/CronPoetry.hpp"
#include "cron/config/CronConfiguration.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using batbox::Json;
int main(){
  for(std::string line;std::getline(std::cin,line);){
    const auto durable=batbox::value::parse(line).at("durable").get<bool>();
    Json output=Json::object();
    output["createDescription"]=batbox::gadgets::cron::buildCronCreateDescription(durable);
    output["createPrompt"]=batbox::gadgets::cron::buildCronCreatePoetry(durable);
    output["deletePrompt"]=batbox::gadgets::cron::buildCronDeletePoetry(durable);
    output["listPrompt"]=batbox::gadgets::cron::buildCronListPoetry(durable);
    output["names"]=Json::array({batbox::gadgets::cron::createName,
      batbox::gadgets::cron::deleteName,batbox::gadgets::cron::listName});
    output["descriptions"]=Json::array({
      batbox::gadgets::cron::deleteDescription,
      batbox::gadgets::cron::listDescription});
    output["maxAgeDays"]=batbox::cron::config::maxAgeDays;
    std::cout<<batbox::value::stringify(output)<<'\n';
  }
}
