#include "terminal_reconciler/CommitLifecycle.hpp"
#include <iostream>

using namespace batbox;
namespace rec=batbox::terminal_reconciler;
namespace td=batbox::terminal_dom;
namespace {
Json run(const std::string& kind,bool test=false,int children=0,
    bool rendered=false,bool log=false,bool start=false,int creates=0){
  Json events=Json::array();
  std::vector<double> times=log?
    std::vector<double>{1000,1050,1060,1081,1090,1105}:
    std::vector<double>{1,2};
  std::size_t at=0;
  auto now=[&](){events.push_back(Json::array({"now"}));
    return times.at(at++);};
  rec::ProfileCounters profile(now);
  rec::CommitLifecycleImports imports;
  if(log)imports.commitLogPath="/tmp/commit.log";
  imports.performanceNow=now;
  imports.appendFileSync=[&](const std::string& path,
      const std::string& value){
    events.push_back(Json::array({"append",path,value}));
  };
  imports.getYogaCounters=[&](){
    events.push_back(Json::array({"counters"}));
    return rec::YogaCounters{1,2,3,4};
  };
  imports.isTestMode=[=](){return test;};
  rec::CommitLifecycle lifecycle(profile,imports);
  auto root=std::make_shared<td::DOMElement>("ink-root");
  root->hasRenderedContent=rendered;
  for(int i=0;i<children;i++)
    root->childNodes.push_back(std::make_shared<td::TextNode>("child"));
  root->onComputeLayout=[&](){events.push_back(Json::array({"compute"}));};
  root->onRender=[&](){events.push_back(Json::array({"render"}));};
  root->onImmediateRender=[&](){
    events.push_back(Json::array({"immediate"}));
  };
  if(start)profile.markCommitStart();
  for(int i=0;i<creates;i++)lifecycle.recordCreatedNode();
  lifecycle.prepareForCommit();lifecycle.resetAfterCommit(*root);
  return Json{{"kind",kind},{"events",events},
    {"lastCommit",profile.getLastCommitMs()},
    {"rendered",root->hasRenderedContent.value_or(false)}};
}
}
int main(){
  Json rows=Json::array();
  rows.push_back(run("production",false,0,false,false,true));
  rows.push_back(run("testEmpty",true));
  rows.push_back(run("testSkipped",true,0,true));
  rows.push_back(run("testChildren",true,1));
  rows.push_back(run("commitLog",false,0,false,true,false,51));
  std::cout<<rows.dump()<<'\n';
}
