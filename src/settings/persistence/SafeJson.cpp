#include "settings/persistence/SafeJson.hpp"
#include <stdexcept>

namespace batbox::settings::persistence {
SafeJsonParser::SafeJsonParser(Logger logError):logError_(std::move(logError)){
  if(!logError_)throw std::invalid_argument("Missing settings JSON error logger");
}
Json SafeJsonParser::parseUncached(const std::string& text,
  bool shouldLogError){
  const auto normalized=text.starts_with("\xef\xbb\xbf")?
    text.substr(3):text;
  try{return value::parse(normalized,true);}
  catch(const std::exception& error){
    if(shouldLogError)logError_(error.what());
    return nullptr;
  }
}
Json SafeJsonParser::safeParseJSON(const std::optional<std::string>& text,
  bool shouldLogError){
  if(!text||text->empty())return nullptr;
  if(value::stringLength(*text)>8192)
    return parseUncached(*text,shouldLogError);
  const std::lock_guard lock(mutex_);
  if(const auto found=index_.find(*text);found!=index_.end()){
    entries_.splice(entries_.begin(),entries_,found->second);
    return found->second->value;
  }
  Json value=parseUncached(*text,shouldLogError);
  entries_.push_front({*text,value});
  index_[*text]=entries_.begin();
  if(entries_.size()>50){
    index_.erase(entries_.back().key);
    entries_.pop_back();
  }
  return value;
}
void SafeJsonParser::clear(){
  const std::lock_guard lock(mutex_);
  entries_.clear();index_.clear();
}
std::size_t SafeJsonParser::stashSize() const{
  const std::lock_guard lock(mutex_);return entries_.size();
}
}
