#pragma once
#include <initializer_list>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

namespace batbox::runtime {
// Indigenous string-key Set: stable insertion order and expected constant-time
// membership/deletion. Appending while iterating retains existing iterators.
class OrderedStringSet {
  using Values=std::list<std::string>;
  Values values_;
  std::unordered_map<std::string,Values::iterator> index_;
public:
  using value_type=std::string;
  using size_type=Values::size_type;
  using iterator=Values::const_iterator;
  using const_iterator=Values::const_iterator;
  OrderedStringSet()=default;
  OrderedStringSet(std::initializer_list<std::string> values){for(const auto& value:values)insert(value);}
  template<class Iterator>OrderedStringSet(Iterator begin,Iterator end){for(;begin!=end;++begin)insert(*begin);}
  OrderedStringSet(const OrderedStringSet& other){for(const auto& value:other)insert(value);}
  OrderedStringSet& operator=(const OrderedStringSet& other){
    if(this!=&other){OrderedStringSet copy(other);swap(copy);}return *this;
  }
  OrderedStringSet(OrderedStringSet&&)=default;
  OrderedStringSet& operator=(OrderedStringSet&&)=default;
  void swap(OrderedStringSet& other)noexcept{values_.swap(other.values_);index_.swap(other.index_);}
  std::pair<iterator,bool> insert(const std::string& value){
    if(const auto found=index_.find(value);found!=index_.end())return {found->second,false};
    values_.push_back(value);auto at=std::prev(values_.end());index_.emplace(value,at);return {at,true};
  }
  bool contains(const std::string& value)const{return index_.contains(value);}
  size_type erase(const std::string& value){
    const auto found=index_.find(value);if(found==index_.end())return 0;
    values_.erase(found->second);index_.erase(found);return 1;
  }
  void clear(){index_.clear();values_.clear();}
  bool empty()const{return values_.empty();}
  size_type size()const{return values_.size();}
  const_iterator begin()const{return values_.begin();}
  const_iterator end()const{return values_.end();}
};
} // namespace batbox::runtime
