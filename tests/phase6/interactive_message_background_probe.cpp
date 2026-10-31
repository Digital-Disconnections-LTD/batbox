#include "interactive/MessageCollapse.hpp"
#include "interactive/MessageTags.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
int main(int argc,char**argv){
 if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);
 Json tags=Json::array(),collapse=Json::array();
 for(const auto&c:cases.at("tags")){
  const auto result=ix::extractTag(c.at("html"),c.at("tag"));
  tags.push_back(Json{{"name",c.at("name")},{"value",result?Json(*result):Json(nullptr)}});
 }
 ix::BackgroundNotificationProtocol protocol{"task-notification","status","summary","Background command "};
 for(const auto&c:cases.at("collapse")){
  ix::MessageRefs refs;for(const auto&m:c.at("messages"))refs.push_back(std::make_shared<Json>(m));
  auto items=ix::asRenderItems(refs);
  auto output=ix::collapseBackgroundBashNotifications(items,c.value("verbose",false),c.at("fullscreen"),protocol);
  Json values=Json::array();for(const auto&item:*output)values.push_back(ix::materializeRenderItem(item));
  collapse.push_back(Json{{"name",c.at("name")},{"same",items==output},{"output",values}});
 }
 std::cout<<Json{{"tags",tags},{"collapse",collapse}}.dump()<<'\n';
}
