#include "cron/CronExpression.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto input=value::parse(line);
    const auto fields=cron::parseCronExpression(
      input["cron"].get<std::string>());
    Json parsed=nullptr,next=nullptr;
    if(fields){
      parsed={{"minute",fields->minute},{"hour",fields->hour},
        {"dayOfMonth",fields->dayOfMonth},{"month",fields->month},
        {"dayOfWeek",fields->dayOfWeek}};
      const auto value=cron::computeNextCronRun(*fields,
        input["fromMs"].get<double>());
      if(value)next=*value;
    }
    std::cout<<value::stringify(Json{{"fields",parsed},{"next",next}})<<'\n';
  }
}
