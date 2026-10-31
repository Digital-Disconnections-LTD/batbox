#include "lsp/Diagnostics.hpp"
#include "lsp/Protocol.hpp"
#include <algorithm>
#include <set>

namespace batbox::lsp {
namespace {
std::string severity(const Json& raw){
  if(!raw.is_number_integer())return "Error";
  switch(raw.get<int>()){case 1:return "Error";case 2:return "Warning";
    case 3:return "Info";case 4:return "Hint";default:return "Error";}
}
int rank(const Json& diag){auto value=diag.value("severity",std::string("Hint"));
  if(value=="Error")return 1;
  if(value=="Warning")return 2;
  if(value=="Info")return 3;
  return 4;}
std::string key(const Json& diag){return Json{{"message",diag.value("message",std::string{})},
  {"severity",diag.value("severity",std::string{})},{"range",diag.value("range",Json())},
  {"source",diag.value("source",Json())},{"code",diag.value("code",Json())}}.dump();}
}
void Diagnostics::publish(const std::string& serverName,const Json& params){
  if(!params.is_object()||!params.contains("uri")||!params.at("uri").is_string()||
     !params.contains("diagnostics")||!params.at("diagnostics").is_array())return;
  Json mapped=Json::array();
  for(const auto& source:params.at("diagnostics")){
    if(mapped.size()>=200)break;
    if(!source.is_object()||!source.contains("message")||!source.contains("range"))continue;
    Json item={{"message",source.at("message")},
      {"severity",severity(source.value("severity",Json()))},{"range",source.at("range")}};
    if(source.contains("source"))item["source"]=source.at("source");
    if(source.contains("code")&&!source.at("code").is_null()){
      const auto& code=source.at("code");item["code"]=code.is_string()?code.get<std::string>():code.dump();}
    mapped.push_back(std::move(item));
  }
  if(mapped.empty())return;
  std::lock_guard lock(mutex_);
  pending_.push_back({{"serverName",serverName},{"files",Json::array({
    Json{{"uri",pathFromFileUri(params.at("uri").get<std::string>()).string()},
         {"diagnostics",std::move(mapped)}}})}});
  while(pending_.size()>500)pending_.pop_front();
}
Json Diagnostics::takeSets(){
  std::lock_guard lock(mutex_);
  if(pending_.empty())return Json::array();
  Json grouped=Json::array();
  std::map<std::string,std::size_t> indexes;
  std::set<std::string> servers;
  while(!pending_.empty()){
    auto batch=std::move(pending_.front());pending_.pop_front();
    servers.insert(batch.value("serverName",std::string{}));
    for(auto& file:batch.at("files")){
      auto uri=file.at("uri").get<std::string>();
      if(!indexes.contains(uri)){indexes[uri]=grouped.size();grouped.push_back({{"uri",uri},{"diagnostics",Json::array()}});}
      auto& dest=grouped[indexes[uri]]["diagnostics"];
      std::set<std::string> seen;
      for(const auto& d:dest)seen.insert(key(d));
      auto delivered=delivered_.find(uri);
      for(const auto& d:file.at("diagnostics")){
        auto identity=key(d);
        if(seen.insert(identity).second&&
           (delivered==delivered_.end()||!delivered->second.contains(identity)))dest.push_back(d);
      }
    }
  }
  Json files=Json::array();int total=0;
  for(auto& file:grouped){
    auto& diagnostics=file["diagnostics"];
    if(diagnostics.empty())continue;
    std::stable_sort(diagnostics.begin(),diagnostics.end(),[](const Json& a,const Json& b){return rank(a)<rank(b);});
    auto remaining=std::max(0,30-total);
    if(static_cast<int>(diagnostics.size())>std::min(10,remaining))
      diagnostics.erase(diagnostics.begin()+std::min(10,remaining),diagnostics.end());
    if(diagnostics.empty())continue;
    auto uri=file.at("uri").get<std::string>();
    if(!delivered_.contains(uri)){deliveredOrder_.push_back(uri);}
    auto& delivered=delivered_[uri];
    for(const auto& d:diagnostics)delivered.insert(key(d));
    if(delivered.size()>128){delivered.clear();for(const auto& d:diagnostics)delivered.insert(key(d));}
    total+=diagnostics.size();files.push_back(std::move(file));
  }
  while(deliveredOrder_.size()>500){delivered_.erase(deliveredOrder_.front());deliveredOrder_.pop_front();}
  if(files.empty())return Json::array();
  std::string names;for(const auto& name:servers){if(!names.empty())names+=", ";names+=name;}
  return Json::array({Json{{"serverName",names},{"files",std::move(files)}}});
}
void Diagnostics::clearDelivered(const std::string& uri){
  auto path=pathFromFileUri(uri).string();
  std::lock_guard lock(mutex_);delivered_.erase(path);
  deliveredOrder_.erase(std::remove(deliveredOrder_.begin(),deliveredOrder_.end(),path),deliveredOrder_.end());
}
void Diagnostics::dropFile(const std::string& uri){
  auto path=pathFromFileUri(uri).string();
  std::lock_guard lock(mutex_);
  delivered_.erase(path);
  deliveredOrder_.erase(std::remove(deliveredOrder_.begin(),deliveredOrder_.end(),path),deliveredOrder_.end());
  for(auto it=pending_.begin();it!=pending_.end();){
    Json kept=Json::array();
    for(const auto& file:it->at("files"))if(file.value("uri",std::string{})!=path)kept.push_back(file);
    if(kept.empty())it=pending_.erase(it);
    else{(*it)["files"]=std::move(kept);++it;}
  }
}
void Diagnostics::reset(){std::lock_guard lock(mutex_);pending_.clear();delivered_.clear();deliveredOrder_.clear();}
}
