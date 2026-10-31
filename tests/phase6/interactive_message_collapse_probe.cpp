#include "interactive/MessageCollapse.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
int main(int argc,char**argv){
 if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json rows=Json::array();
 for(const auto&spec:cases){ix::MessageRefs sources;
  for(const auto&m:spec.at("messages"))sources.push_back(std::make_shared<Json>(m));
  auto inputItems=ix::asRenderItems(sources);
  auto output=spec.at("action")=="team"?ix::collapsePartnerShutdowns(inputItems):ix::collapseHookSummaries(inputItems);
  Json values=Json::array(),retained=Json::array();
  for(const auto&item:*output){values.push_back(ix::materializeRenderItem(item));
   const auto found=std::find(sources.begin(),sources.end(),item.message);
   retained.push_back(found==sources.end()?-1:static_cast<int>(found-sources.begin()));}
  rows.push_back(Json{{"name",spec.at("name")},{"output",values},{"retained",retained}});
 }
 std::cout<<rows.dump()<<'\n';
}
