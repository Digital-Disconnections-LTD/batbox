#include "interactive/text_input/Cursor.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/uchar.h>

namespace batbox::interactive::text_input {
namespace {
bool jsSpace(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
}
bool isVimWhitespace(const std::string& value){
  for(const auto unit:value::utf16CodeUnits(value))if(jsSpace(unit))return true;
  return false;
}
bool isVimWordChar(const std::string& value){
  const auto units=value::utf16CodeUnits(value);
  if(units.empty())return false;
  UChar32 codepoint=units.front();
  if(codepoint>=0xd800&&codepoint<=0xdbff&&units.size()==2&&
     units[1]>=0xdc00&&units[1]<=0xdfff)
    codepoint=0x10000+((codepoint-0xd800)<<10)+(units[1]-0xdc00);
  else if(units.size()!=1)return false;
  if(codepoint==u'_')return true;
  const auto category=u_charType(codepoint);
  return (category>=U_UPPERCASE_LETTER&&category<=U_OTHER_LETTER)||
    (category>=U_NON_SPACING_MARK&&category<=U_COMBINING_SPACING_MARK)||
    (category>=U_DECIMAL_DIGIT_NUMBER&&category<=U_OTHER_NUMBER);
}
bool isVimPunctuation(const std::string& value){
  return !value.empty()&&!isVimWhitespace(value)&&!isVimWordChar(value);
}
Cursor Cursor::nextWord()const{
  if(isAtEnd())return *this;
  for(const auto& boundary:measured_->getWordBoundaries())
    if(boundary.isWordLike&&boundary.start>offset_)
      return Cursor(measured_,snapOutOfReference(boundary.start,boundary.start<offset_));
  return Cursor(measured_,measured_->length());
}
Cursor Cursor::endOfWord()const{
  if(isAtEnd())return *this;
  const auto& boundaries=measured_->getWordBoundaries();
  for(const auto& boundary:boundaries){
    if(!boundary.isWordLike)continue;
    if(offset_>=boundary.start&&offset_<boundary.end-1)
      return Cursor(measured_,snapOutOfReference(boundary.end-1,false));
    if(offset_==boundary.end-1){
      for(const auto& next:boundaries)
        if(next.isWordLike&&next.start>offset_)
          return Cursor(measured_,snapOutOfReference(next.end-1,false));
      return *this;
    }
  }
  for(const auto& boundary:boundaries)
    if(boundary.isWordLike&&boundary.start>offset_)
      return Cursor(measured_,snapOutOfReference(boundary.end-1,false));
  return *this;
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
Cursor Cursor::nextVimWord()const{
  if(isAtEnd())return *this;
  auto position=offset_;
  const auto current=graphemeAt(position);
  if(current.empty())return *this;
  if(isVimWordChar(current)){
    while(position<measured_->length()&&isVimWordChar(graphemeAt(position)))
      position=measured_->nextOffset(position);
  }else if(isVimPunctuation(current)){
    while(position<measured_->length()&&isVimPunctuation(graphemeAt(position)))
      position=measured_->nextOffset(position);
  }
  while(position<measured_->length()&&isVimWhitespace(graphemeAt(position)))
    position=measured_->nextOffset(position);
  return Cursor(measured_,snapOutOfReference(position,false));
}
Cursor Cursor::endOfVimWord()const{
  if(isAtEnd())return *this;
  auto position=offset_;
  if(graphemeAt(position).empty())return *this;
  position=measured_->nextOffset(position);
  while(position<measured_->length()&&isVimWhitespace(graphemeAt(position)))
    position=measured_->nextOffset(position);
  if(position>=measured_->length())return Cursor(measured_,measured_->length());
  const auto current=graphemeAt(position);
  if(isVimWordChar(current)||isVimPunctuation(current)){
    const bool word=isVimWordChar(current);
    while(position<measured_->length()){
      const auto next=measured_->nextOffset(position);
      if(next>=measured_->length()||
         (word?!isVimWordChar(graphemeAt(next)):
                !isVimPunctuation(graphemeAt(next))))break;
      position=next;
    }
  }
  return Cursor(measured_,snapOutOfReference(position,false));
}
Cursor Cursor::prevVimWord()const{
  if(isAtStart())return *this;
  auto position=measured_->prevOffset(offset_);
  while(position>0&&isVimWhitespace(graphemeAt(position)))
    position=measured_->prevOffset(position);
  if(position==0&&isVimWhitespace(graphemeAt(0)))
    return Cursor(measured_,0);
  const auto current=graphemeAt(position);
  if(isVimWordChar(current)||isVimPunctuation(current)){
    const bool word=isVimWordChar(current);
    while(position>0){
      const auto previous=measured_->prevOffset(position);
      if(word?!isVimWordChar(graphemeAt(previous)):
              !isVimPunctuation(graphemeAt(previous)))break;
      position=previous;
    }
  }
  return Cursor(measured_,position);
}
Cursor Cursor::nextWORD()const{
  auto cursor=*this;
  while(!cursor.isOverWhitespace()&&!cursor.isAtEnd())cursor=cursor.right();
  while(cursor.isOverWhitespace()&&!cursor.isAtEnd())cursor=cursor.right();
  return cursor;
}
Cursor Cursor::endOfWORD()const{
  if(isAtEnd())return *this;
  auto cursor=*this;
  const bool atEnd=!cursor.isOverWhitespace()&&
    (cursor.right().isOverWhitespace()||cursor.right().isAtEnd());
  if(atEnd)return cursor.right().endOfWORD();
  if(cursor.isOverWhitespace())cursor=cursor.nextWORD();
  while(!cursor.right().isOverWhitespace()&&!cursor.isAtEnd())
    cursor=cursor.right();
  return cursor;
}
Cursor Cursor::prevWORD()const{
  auto cursor=*this;
  if(cursor.left().isOverWhitespace())cursor=cursor.left();
  while(cursor.isOverWhitespace()&&!cursor.isAtStart())cursor=cursor.left();
  if(!cursor.isOverWhitespace())
    while(!cursor.left().isOverWhitespace()&&!cursor.isAtStart())
      cursor=cursor.left();
  return cursor;
}
} // namespace batbox::interactive::text_input
