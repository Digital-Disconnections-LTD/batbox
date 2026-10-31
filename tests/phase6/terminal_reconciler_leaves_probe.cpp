#include "terminal_reconciler/HostLeaves.hpp"
#include "FakeFullLayout.hpp"
#include <iostream>

using namespace batbox;
namespace rec=batbox::terminal_reconciler;
namespace td=batbox::terminal_dom;
namespace {
Json fields(const rec::PropsRef& result){
  if(!result)return nullptr;
  Json array=Json::array();
  for(const auto& key:result->keys()){
    const auto value=result->get(key);
    array.push_back(Json::array({key,value?*value:Json("@undefined")}));
  }
  return array;
}
void diffRow(Json& rows,const std::string& kind,const rec::PropsRef& before,
    const rec::PropsRef& after){
  const auto result=rec::diff(before,after);
  rows.push_back(Json{{"kind",kind},{"fields",fields(result)},
    {"sameAfter",result==after}});
}
rec::PropsRef props(std::initializer_list<std::pair<std::string,Json>> values){
  auto result=std::make_shared<rec::Props>();
  for(const auto& [key,value]:values)result->set(key,value);
  return result;
}
struct TracedLayout:FakeFullLayout {
  Json* log=nullptr;
  td::DOMElement* node=nullptr;
  void unsetMeasureFunc()override{
    log->push_back(Json::array({"unset",bool(node->yogaNode)}));
  }
  void freeRecursive()override{
    log->push_back(Json::array({"free",bool(node->yogaNode)}));
  }
};
rec::OwnerFiberRef fiber(rec::OwnerFiber::Type type,
    std::optional<std::string> name,
    std::optional<std::string> displayName={}){
  auto result=std::make_shared<rec::OwnerFiber>();
  result->type=type;result->name=std::move(name);
  result->displayName=std::move(displayName);
  return result;
}
}
int main(){
  Json rows=Json::array();
  auto same=props({{"a",1}});diffRow(rows,"diffSame",same,same);
  diffRow(rows,"diffNoBefore",{},props({{"a",1}}));
  diffRow(rows,"diffDelete",props({{"a",1},{"b",2}}),
    props({{"a",1}}));
  diffRow(rows,"diffAdd",props({{"a",1}}),
    props({{"a",1},{"b",2}}));
  diffRow(rows,"diffChange",props({{"a",1},{"b",false}}),
    props({{"a",2},{"b",false}}));
  auto nested=std::make_shared<Json>(Json{{"x",1}});
  auto before=std::make_shared<rec::Props>();before->setRef("a",nested);
  auto after=std::make_shared<rec::Props>();after->setRef("a",nested);
  diffRow(rows,"diffNestedSame",before,after);
  diffRow(rows,"diffNestedNew",props({{"a",Json{{"x",1}}}}),
    props({{"a",Json{{"x",1}}}}));
  diffRow(rows,"diffAllDelete",props({{"a",1},{"b",2}}),{});
  {
    Json log=Json::array();
    auto node=std::make_shared<td::DOMElement>("ink-box");
    auto yoga=std::make_shared<TracedLayout>();
    yoga->log=&log;yoga->node=node.get();node->yogaNode=yoga;
    rec::cleanupYogaNode(*node);
    rows.push_back(Json{{"kind","cleanup"},{"log",log},
      {"cleared",!node->yogaNode}});
  }
  {
    using Type=rec::OwnerFiber::Type;
    auto root=fiber(Type::Object,"Root");
    auto host=fiber(Type::String,"ink-box");host->parent=root;
    auto leaf=fiber(Type::Function,"Comp","Pane");
    leaf->debugOwner=host;leaf->parent=fiber(Type::Object,"Ignored");
    rows.push_back(Json{{"kind","owner"},
      {"chain",rec::getOwnerChain(leaf)}});
    auto a=fiber(Type::Object,"Repeat");
    auto b=fiber(Type::Object,"Repeat");b->parent=a;a->parent=b;
    rows.push_back(Json{{"kind","ownerCycle"},
      {"chain",rec::getOwnerChain(b)}});
    auto longChain=fiber(Type::Object,"End");
    for(int i=0;i<60;i++){
      auto next=fiber(Type::Object,"N"+std::to_string(i));
      next->parent=longChain;longChain=next;
    }
    const auto chain=rec::getOwnerChain(longChain);
    rows.push_back(Json{{"kind","ownerLimit"},
      {"length",chain.size()},{"first",chain.front()},
      {"last",chain.back()}});
  }
  {
    auto value=std::optional<std::string>("yes");
    rec::DebugRepaints debug([&](){return value;});
    rows.push_back(Json{{"kind","debugFirst"},{"value",debug.enabled()}});
    value="false";
    rows.push_back(Json{{"kind","debugCached"},
      {"value",debug.enabled()}});
  }
  {
    double tick=100,last=0;
    rec::ProfileCounters profile([&](){last=tick++;return last;});
    profile.recordYogaMs(5);profile.markCommitStart();
    rows.push_back(Json{{"kind","profileBefore"},
      {"yoga",profile.getLastYogaMs()},
      {"commit",profile.getLastCommitMs()},{"start",last}});
    profile.resetProfileCounters();
    rows.push_back(Json{{"kind","profileAfter"},
      {"yoga",profile.getLastYogaMs()},
      {"commit",profile.getLastCommitMs()},{"start",0}});
  }
  std::cout<<rows.dump()<<'\n';
}
