#include "interactive/MessageLookups.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix=batbox::interactive;
namespace {
template<class T,class F>Json pairs(const std::unordered_map<std::string,T>& map,F f){
 std::vector<std::string> keys;for(const auto&[key,_]:map)keys.push_back(key);std::sort(keys.begin(),keys.end());
 Json out=Json::array();for(const auto&key:keys)out.push_back(Json::array({key,f(map.at(key))}));return out;
}
Json counts(const std::unordered_map<std::string,std::unordered_map<std::string,int>>& map){
 return pairs(map,[](const auto&inner){return pairs(inner,[](int value){return Json(value);});});
}
Json set(const ix::GadgetIDs& ids){std::vector<std::string> v(ids.begin(),ids.end());std::sort(v.begin(),v.end());return v;}
ix::MessageRefs refs(const Json& input){ix::MessageRefs out;for(const auto&v:input)out.push_back(std::make_shared<Json>(v));return out;}
} // namespace
int main(int argc,char**argv){if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json out=Json::array();
 for(const auto&spec:cases){auto shown=refs(spec.at("shown")),normalized=refs(spec.at("normalized"));
  const auto l=ix::buildMessageLookups(normalized,shown);
  bool shared=true;
  for(const auto&[id,siblings]:l.siblingGadgetUseIDs)for(const auto&other:*siblings)
   if(l.siblingGadgetUseIDs.at(other)!=siblings)shared=false;
  out.push_back(Json{{"name",spec.at("name")},
   {"siblings",pairs(l.siblingGadgetUseIDs,[](const auto&s){return Json(std::vector<std::string>(s->begin(),s->end()));})},
   {"progress",pairs(l.progressMessagesByGadgetUseID,[](const auto&v){Json a=Json::array();for(const auto&m:v)a.push_back(m->at("uuid"));return a;})},
   {"inProgress",counts(l.inProgressHookCounts)},{"resolvedHooks",counts(l.resolvedHookCounts)},
   {"results",pairs(l.gadgetResultByGadgetUseID,[](const auto&m){return m->at("uuid");})},
   {"uses",pairs(l.gadgetUseByGadgetUseID,[](const auto&b){return Json(b.value().at("name"));})},
   {"count",l.normalizedMessageCount},{"resolved",set(l.resolvedGadgetUseIDs)},
   {"errored",set(l.erroredGadgetUseIDs)},{"sharedSibling",shared}});
 }
 std::cout<<out.dump()<<'\n';
}
