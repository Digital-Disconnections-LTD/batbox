#include "interactive/MessageDecisions.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
namespace ix = batbox::interactive;
namespace {
ix::GadgetIDs ids(const Json& spec, const char* key) {
  ix::GadgetIDs out;
  if (spec.contains(key)) for (const auto& id : spec.at(key)) out.insert(id.get<std::string>());
  return out;
}
void counts(const Json& spec, const char* key,
            std::unordered_map<std::string,std::unordered_map<std::string,int>>& out) {
  if (!spec.contains(key)) return;
  for (auto it=spec.at(key).begin();it!=spec.at(key).end();++it)
    for (auto jt=it.value().begin();jt!=it.value().end();++jt)
      out[it.key()][jt.key()]=jt.value().get<int>();
}
} // namespace
int main(int argc,char** argv) {
  if (argc!=3 || std::string(argv[1])!="--cases") return 2;
  std::ifstream input(argv[2]); const Json cases=Json::parse(input); Json result=Json::array();
  for (const auto& spec:cases) {
    const auto action=spec.at("action").get<std::string>();
    const auto streaming=ids(spec,"streaming"), resolved=ids(spec,"resolved");
    Json value;
    if (action=="after") {
      ix::MessageRefs messages;
      for (const auto& message:spec.at("messages")) messages.push_back(std::make_shared<Json>(message));
      ix::MessageDecisionImports imports{[](const std::string& name,const Json&){
        return name=="Read"||name=="Search";}};
      value=ix::hasContentAfterIndex(messages,spec.at("index"),streaming,imports);
    } else if (action=="streaming") value=ix::isMessageStreaming(spec.at("message"),streaming);
    else if (action=="resolved") value=ix::allGadgetsResolved(spec.at("message"),resolved);
    else if (action=="static") {
      ix::MessageDecisionLookups lookups; lookups.resolvedGadgetUseIDs=resolved;
      counts(spec,"hooks",lookups.inProgressHookCounts);
      counts(spec,"resolvedHooks",lookups.resolvedHookCounts);
      value=ix::shouldRenderStatically(spec.at("message"),streaming,ids(spec,"inProgress"),
        ids(spec,"siblings"),spec.at("screen"),lookups);
    } else value=ix::expandKey(spec.at("message"));
    result.push_back(Json{{"name",spec.at("name")},{"result",value}});
  }
  std::cout<<result.dump()<<'\n';
}
