#include "cwd/Override.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){using namespace batbox;cwd::State state{"/original","/global","/global"};event_loop::Loop loop;event_loop::Deferred<event_loop::Unit> pending(loop);Json rows=Json::object();
 for(const auto* id:{"A","B","C","D"}){const std::string name=id;cwd::runWithCwdOverride("/"+name,[&]{pending.then([&,name](const auto*,const auto&){rows[name]=cwd::getCwd(state);});});}
 cwd::runWithCwdOverride("",[&]{pending.then([&](const auto*,const auto&){rows["empty"]=cwd::getCwd(state);});});state.cwd="/new";pending.resolve({});loop.runReady();rows["outside"]=cwd::getCwd(state);std::cout<<rows.dump()<<'\n';}
