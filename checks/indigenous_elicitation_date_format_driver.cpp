#include "application_mcp_elicitation/DateTimeParser.hpp"
#include <iostream>
#include <iterator>

int main(){
  using batbox::Json;
  std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=Json::parse(input);
  Json output=Json::array();
  using namespace std::chrono;
  const auto fixed=sys_days(year(2025)/October/14)+hours(10)+minutes(30);
  const auto context=batbox::application_mcp_elicitation::currentDateTimeContext(fixed);
  for(const auto& item:cases){
    const auto value=item.at("input").get<std::string>();
    if(item.at("operation")=="looks"){
      output.push_back({{"looks",
        batbox::application_mcp_elicitation::looksLikeISO8601(value)}});
      continue;
    }
    const auto poetry=batbox::application_mcp_elicitation::buildDateTimePoetry(
      value,item.at("format").get<std::string>(),context);
    auto result=item.value("throw",false)
      ?batbox::application_mcp_elicitation::DateTimeParseResult{
        false,{},"Unable to parse date/time. Please enter in ISO 8601 format manually."}
      :batbox::application_mcp_elicitation::parseDateTimeModelText(
        item.value("modelText",std::string{}));
    Json response={{"success",result.success}};
    if(result.success)response["value"]=result.value;
    else response["error"]=result.error;
    output.push_back({{"result",response},{"prompt",Json{
      {"systemPrompt",poetry.systemPoetry},{"userPrompt",poetry.userPoetry}}}});
  }
  std::cout<<output.dump()<<'\n';
}
