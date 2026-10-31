#include "interactive/viewport/Range.hpp"
#include <algorithm>
#include <unordered_set>

namespace batbox::interactive::viewport {
void RangeModel::setKeys(std::vector<std::string> keys){
  const bool append=keys.size()>=keys_.size()&&std::equal(keys_.begin(),keys_.end(),keys.begin());
  if(!append){
    const std::unordered_set<std::string> live(keys.begin(),keys.end());
    bool dirty=false;
    for(auto it=heights_.begin();it!=heights_.end();)
      if(!live.contains(it->first)){it=heights_.erase(it);dirty=true;}else ++it;
    if(dirty)++version_;
  }
  keys_=std::move(keys);
}
std::shared_ptr<Geometry> RangeModel::mount(const std::string& key){
  auto& result=mounted_[key];if(!result)result=std::make_shared<Geometry>();return result;
}
void RangeModel::measure(const std::string& key,const Geometry& geometry){
  const auto found=heights_.find(key);
  if(geometry.height>0||(geometry.width>0&&geometry.height==0))
    if(found==heights_.end()||found->second!=geometry.height){heights_[key]=geometry.height;++version_;}
}
void RangeModel::unmount(const std::string& key){
  const auto found=mounted_.find(key);if(found==mounted_.end())return;
  if(!skipMeasurement_)measure(key,*found->second);
  mounted_.erase(found);
}
void RangeModel::committed(double listOrigin){
  listOrigin_=listOrigin;
  if(skipMeasurement_){skipMeasurement_=false;return;}
  for(const auto& [key,geometry]:mounted_)measure(key,*geometry);
}
void RangeModel::rebuildOffsets(){
  if(offsetVersion_==version_&&offsetCount_==keys_.size())return;
  if(offsets_.size()<keys_.size()+1)offsets_.resize(keys_.size()+1);
  offsets_[0]=0;
  for(std::size_t i=0;i<keys_.size();++i){
    const auto found=heights_.find(keys_[i]);
    offsets_[i+1]=offsets_[i]+(found==heights_.end()?3:found->second);
  }
  offsetVersion_=version_;offsetCount_=keys_.size();
}
std::shared_ptr<Geometry> RangeModel::itemGeometry(std::size_t index)const{
  if(index>=keys_.size())return {};
  const auto found=mounted_.find(keys_[index]);return found==mounted_.end()?nullptr:found->second;
}
double RangeModel::itemTop(std::size_t index)const{
  const auto geometry=itemGeometry(index);return !geometry||geometry->width==0?-1:geometry->top;
}
std::optional<double> RangeModel::itemHeight(std::size_t index)const{
  if(index>=keys_.size())return {};
  const auto found=heights_.find(keys_[index]);return found==heights_.end()?std::nullopt:std::optional<double>(found->second);
}
double RangeModel::offset(std::size_t index)const{return index<offsets_.size()?offsets_[index]:0;}
} // namespace batbox::interactive::viewport
