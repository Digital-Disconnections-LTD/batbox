#include "flights/WriteQueue.hpp"
#include <filesystem>
#include <iostream>
#include <set>
using namespace batbox;
Json evaluate(const Json& t){
 Json trace=Json::array();std::set<std::string> fail,always;
 for(const auto& f:t.value("failOnce",Json::array()))fail.insert(f);
 for(const auto& f:t.value("failAlways",Json::array()))always.insert(f);
 flights::QueueServices services;
 services.appendFile=[&](const std::string& file,const std::string& content){trace.push_back({"append",file,content,0600});if(fail.erase(file)||always.contains(file))throw std::runtime_error("write failed");};
 services.mkdir=[&](const std::string& file){trace.push_back({"mkdir",file,0700});};
 flights::WriteQueue q(services,std::chrono::seconds(30),t.value("maxChunk",std::size_t(104857600)));
 std::vector<std::shared_ptr<Json>> entries;std::vector<std::shared_future<void>> futures;
 for(const auto& entry:t.value("entries",Json::array())){
  auto value=std::make_shared<Json>(entry.at("value"));entries.push_back(value);
  futures.push_back(q.enqueueWrite(entry.at("file"),flights::WriteQueue::EntryReader([value]{return *value;})));
 }
 for(const auto& edit:t.value("edits",Json::array()))for(const auto& [key,value]:edit.at("fields").items())(*entries.at(edit.at("index").get<std::size_t>()))[key]=value;
 Json error;try{q.flush();}catch(const std::exception& e){error=e.what();}
 Json done=Json::array();
 // Resolve order follows file/chunk processing, not global enqueue order.
 // Indigenous futures expose completion state; append ordering is checked through the trace.
 for(std::size_t i=0;i<futures.size();++i)if(futures[i].wait_for(std::chrono::seconds(0))==std::future_status::ready)try{futures[i].get();done.push_back(i);}catch(const std::future_error&){}
 return Json{{"trace",trace},{"done",done},{"error",error}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{const auto result=evaluate(value::parse(line));std::cout<<value::stringify(Json{{"result",result}})<<'\n';}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}}
