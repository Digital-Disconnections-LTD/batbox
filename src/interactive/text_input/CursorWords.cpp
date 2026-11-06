#include "interactive/text_input/Cursor.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive::text_input {
Cursor Cursor::nextWord()const{
  if(isAtEnd())return *this;
  for(const auto& boundary:measured_->getWordBoundaries())
    if(boundary.isWordLike&&boundary.start>offset_)
      return Cursor(measured_,snapOutOfReference(boundary.start,boundary.start<offset_));
  return Cursor(measured_,measured_->length());
}
Cursor Cursor::prevWord()const{
  if(isAtStart())return *this;
  std::optional<std::size_t> previous;
  for(const auto& boundary:measured_->getWordBoundaries()){
    if(!boundary.isWordLike)continue;
    if(boundary.start<offset_){
      if(offset_>boundary.start&&offset_<=boundary.end)
        return Cursor(measured_,snapOutOfReference(boundary.start,boundary.start<offset_));
      previous=boundary.start;
    }
  }
  return Cursor(measured_,previous.value_or(0));
}
} // namespace batbox::interactive::text_input
