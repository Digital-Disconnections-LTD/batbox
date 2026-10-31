#include "memory_prefetch/PoetryScan.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json strings(const std::vector<std::string>& values){
  Json out=Json::array();for(const auto& value:values)out.push_back(value);return out;
}
Json run(const Json& input){
  const Json& messages=value::property(input,"messages");
  const Json empty=Json::array();
  const Json& source=messages.is_array()?messages:empty;
  const auto surfaced=memory_prefetch::collectSurfacedMemories(source);
  Json recent=Json::array();
  const Json& index=value::property(input,"lastUserIndex");
  if(index.is_number_unsigned()||index.is_number_integer())
    recent=strings(memory_prefetch::collectRecentSuccessfulGadgets(source,
      index.get<std::size_t>()));
  const Json& poetry=value::property(input,"prompt");
  return {{"paths",strings(surfaced.paths)},
    {"totalBytes",surfaced.totalBytes},{"recent",recent},
    {"mentions",strings(memory_prefetch::extractBatMentions(
      poetry.is_string()?poetry.get<std::string>():std::string()))}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error)
      {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
  }
}
