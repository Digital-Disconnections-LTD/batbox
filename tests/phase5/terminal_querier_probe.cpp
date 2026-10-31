#include "termio/Querier.hpp"
#include <iostream>
#include <map>
using namespace batbox;using namespace batbox::termio;
TerminalQuery build(const Json& op,const std::string& terminal){const auto name=op.at("name").get<std::string>();const auto args=op.value("args",Json::array());
 if(name=="decrqm")return query::decrqm(args.at(0));
 if(name=="da1")return query::da1();
 if(name=="da2")return query::da2();
 if(name=="kittyKeyboard")return query::kittyKeyboard();
 if(name=="cursorPosition")return query::cursorPosition();
 if(name=="oscColor")return query::oscColor(args.at(0),terminal);
 return query::xtversion();}
int main(){Json items;std::cin>>items;Json all=Json::array();for(const auto& item:items){event_loop::Loop loop;Json trace=Json::array(),writes=Json::array();bool fail=false;std::map<std::string,ResponseRef> saved;TerminalQuerier t(loop,[&](const std::string& text){writes.push_back(text);if(fail)throw std::runtime_error("write failed");});
 for(const auto& op:item.at("steps")){try{const auto kind=op.at("op").get<std::string>();
  if(kind=="send"){auto q=op.contains("custom")?TerminalQuery{op.at("custom"),[op](const ResponseRef& r){if(op.value("throwMatch",false))throw std::runtime_error("match failed");return value::property(*r,"type")==op.at("type");}}:build(op,item.value("terminal",""));
   t.send(std::move(q)).then([&,id=op.at("id")](const ResponseRef* r,const std::exception_ptr& error){if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back(Json::array({id,Json{{"error",e.what()}}}));}}else trace.push_back(Json::array({id,*r?**r:Json(nullptr)}));});
  }else if(kind=="flush")t.flush().then([&,id=op.at("id")](const event_loop::Unit*,const std::exception_ptr& error){if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back(Json::array({id,Json{{"error",e.what()}}}));}}else trace.push_back(Json::array({id,"flush"}));});
  else if(kind=="response"){auto r=std::make_shared<Json>(op.at("value"));saved[op.value("id","last")]=r;t.onResponse(r);}
  else if(kind=="mutate"){auto r=saved.at(op.value("id","last"));for(auto it=op.at("value").begin();it!=op.at("value").end();++it)(*r)[it.key()]=it.value();}
  else if(kind=="writeFail")fail=op.at("value");else if(kind=="tick")loop.runReady();
  trace.push_back(Json::array({"sync",kind}));
 }catch(const std::exception& e){trace.push_back(Json::array({"throw",e.what()}));}}
 loop.runReady();all.push_back(Json{{"trace",trace},{"writes",writes}});}std::cout<<value::stringify(all)<<'\n';}
