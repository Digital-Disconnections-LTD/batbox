#include "settings/Stash.hpp"

namespace batbox::settings {
std::optional<Json> Stash::getFlightSettingsStash() const{
  return flightSettingsStash_;
}
void Stash::setFlightSettingsStash(const Json& value){
  flightSettingsStash_=value;
}
std::optional<Json> Stash::getStashedSettingsForSource(Source source) const{
  const auto found=perSourceStash_.find(source);
  if(found==perSourceStash_.end())return std::nullopt;
  return found->second;
}
void Stash::setStashedSettingsForSource(Source source,const Json& value){
  perSourceStash_[source]=value;
}
std::optional<ParsedSettings> Stash::getStashedParsedFile(
  const std::string& path) const{
  const auto found=parseFileStash_.find(path);
  if(found==parseFileStash_.end())return std::nullopt;
  return found->second;
}
void Stash::setStashedParsedFile(const std::string& path,
  const ParsedSettings& value){parseFileStash_[path]=value;}
void Stash::resetSettingsStash(){
  flightSettingsStash_.reset();
  perSourceStash_.clear();
  parseFileStash_.clear();
}
std::optional<Json> Stash::getPluginSettingsBase() const{
  return pluginSettingsBase_;
}
void Stash::setPluginSettingsBase(const std::optional<Json>& settings){
  pluginSettingsBase_=settings;
}
void Stash::clearPluginSettingsBase(){pluginSettingsBase_.reset();}
}
