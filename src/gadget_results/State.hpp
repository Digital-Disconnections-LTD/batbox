#pragma once
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace batbox::gadget_results {
// Set and Map retain insertion order. State is shared across query turns and
// explicitly copied at a stash-sharing fork.
class SeenIds {
  std::vector<std::string> entries_;
  std::unordered_set<std::string> lookup_;
public:
  void add(const std::string& id){if(lookup_.insert(id).second)entries_.push_back(id);}
  bool contains(const std::string& id)const{return lookup_.contains(id);}
  const auto& entries()const{return entries_;}
  std::size_t size()const{return entries_.size();}
};
class ReplacementMap {
  using Entries=std::list<std::pair<std::string,std::string>>;
  Entries entries_;
  std::unordered_map<std::string,Entries::iterator> lookup_;
public:
  ReplacementMap()=default;
  ReplacementMap(const ReplacementMap& other){for(const auto& [id,text]:other.entries_)set(id,text);}
  ReplacementMap& operator=(const ReplacementMap& other){
    if(this!=&other){ReplacementMap copy(other);swap(copy);}return *this;
  }
  ReplacementMap(ReplacementMap&&)=default;
  ReplacementMap& operator=(ReplacementMap&&)=default;
  void swap(ReplacementMap& other)noexcept{entries_.swap(other.entries_);lookup_.swap(other.lookup_);}
  void set(const std::string& id,const std::string& text){
    if(const auto found=lookup_.find(id);found!=lookup_.end()){found->second->second=text;return;}
    entries_.emplace_back(id,text);lookup_.emplace(id,std::prev(entries_.end()));
  }
  bool contains(const std::string& id)const{return lookup_.contains(id);}
  const std::string* get(const std::string& id)const{
    const auto found=lookup_.find(id);return found==lookup_.end()?nullptr:&found->second->second;
  }
  const auto& entries()const{return entries_;}
  std::size_t size()const{return entries_.size();}
};
struct ContentReplacementState {
  SeenIds seenIds;
  ReplacementMap replacements;
};
inline std::shared_ptr<ContentReplacementState> createContentReplacementState(){
  return std::make_shared<ContentReplacementState>();
}
inline std::shared_ptr<ContentReplacementState> cloneContentReplacementState(
  const ContentReplacementState& source){
  return std::make_shared<ContentReplacementState>(source);
}
} // namespace batbox::gadget_results
