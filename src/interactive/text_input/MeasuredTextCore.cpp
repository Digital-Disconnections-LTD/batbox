#include "interactive/text_input/MeasuredText.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::interactive::text_input {
namespace {
void require(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Text metric required: ")+name);
}
}
std::size_t WrappedLine::length()const{
  return value::stringLength(text)+(endsWithNewline?1:0);
}
MeasuredText::MeasuredText(std::string text,double columns,
                           MetricsServices services):
    text_(services.normalizeNfc?services.normalizeNfc(text):std::string{}),
    units_(value::utf16CodeUnits(text_)),columns_(columns),
    length_(units_.size()),services_(std::move(services)){
  require(services_.normalizeNfc,"normalizeNfc");
  require(services_.stringWidth,"stringWidth");
  require(services_.wrapAnsi,"wrapAnsi");
  require(services_.grapheme,"grapheme");
  require(services_.word,"word");
}
const std::vector<WordBoundary>& MeasuredText::getWordBoundaries()const{
  if(!wordBoundaries_){
    wordBoundaries_.emplace();
    auto cursor=services_.word->segment(text_);
    while(const auto item=cursor.next())
      wordBoundaries_->push_back({item->index,
        item->index+value::stringLength(item->text),
        item->isWordLike.value_or(false)});
  }
  return *wordBoundaries_;
}
const std::vector<std::size_t>& MeasuredText::graphemeBoundaries()const{
  if(!graphemeBoundaries_){
    graphemeBoundaries_.emplace();
    auto cursor=services_.grapheme->segment(text_);
    while(const auto item=cursor.next())
      graphemeBoundaries_->push_back(item->index);
    graphemeBoundaries_->push_back(length_);
  }
  return *graphemeBoundaries_;
}
std::size_t MeasuredText::binarySearchBoundary(
    const std::vector<std::size_t>& boundaries,std::size_t target,
    bool findNext)const{
  std::size_t left=0,right=boundaries.size();
  std::size_t result=findNext?length_:0;
  while(left<right){
    const auto mid=left+(right-left)/2;
    const auto boundary=boundaries[mid];
    if(findNext){
      if(boundary>target){result=boundary;right=mid;}
      else left=mid+1;
    }else{
      if(boundary<target){result=boundary;left=mid+1;}
      else right=mid;
    }
  }
  return result;
}
std::size_t MeasuredText::nextOffset(std::size_t offset)const{
  return binarySearchBoundary(graphemeBoundaries(),offset,true);
}
std::size_t MeasuredText::prevOffset(std::size_t offset)const{
  if(offset==0)return 0;
  return binarySearchBoundary(graphemeBoundaries(),offset,false);
}
std::size_t MeasuredText::snapToGraphemeBoundary(std::size_t offset)const{
  if(offset==0)return 0;
  if(offset>=length_)return length_;
  const auto& boundaries=graphemeBoundaries();
  const auto found=std::upper_bound(boundaries.begin(),boundaries.end(),offset);
  return *(found-1);
}
double MeasuredText::stringIndexToDisplayWidth(const std::string& text,
                                                std::size_t index)const{
  if(index==0)return 0;
  if(index>=value::stringLength(text))return services_.stringWidth(text);
  return services_.stringWidth(value::substring(text,0,static_cast<double>(index)));
}
std::size_t MeasuredText::offsetAtDisplayWidth(double targetWidth)const{
  if(targetWidth<=0)return 0;
  double currentWidth=0;
  const auto& boundaries=graphemeBoundaries();
  for(std::size_t i=0;i+1<boundaries.size();++i){
    const auto start=boundaries[i],end=boundaries[i+1];
    const auto segment=value::substring(text_,static_cast<double>(start),
                                     static_cast<double>(end));
    const double segmentWidth=services_.stringWidth(segment);
    if(currentWidth+segmentWidth>targetWidth)return start;
    currentWidth+=segmentWidth;
  }
  return length_;
}
std::size_t MeasuredText::displayWidthToStringIndex(
    const std::string& text,double targetWidth)const{
  if(targetWidth<=0||text.empty())return 0;
  if(text==text_)return offsetAtDisplayWidth(targetWidth);
  double currentWidth=0;
  std::size_t currentOffset=0;
  auto cursor=services_.grapheme->segment(text);
  while(const auto item=cursor.next()){
    const double segmentWidth=services_.stringWidth(item->text);
    if(currentWidth+segmentWidth>targetWidth)break;
    currentWidth+=segmentWidth;
    currentOffset=item->index+value::stringLength(item->text);
  }
  return currentOffset;
}
} // namespace batbox::interactive::text_input
