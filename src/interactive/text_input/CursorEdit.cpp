#include "interactive/text_input/Cursor.hpp"
#include "runtime/JsonValue.hpp"
#include <regex>

namespace batbox::interactive::text_input {
Cursor Cursor::modifyText(const Cursor& end,
                           const std::string& insertString)const{
  const auto nextText=value::slice(text(),0,offset_)+insertString+
    value::slice(text(),end.offset_);
  return fromText(nextText,columns(),measured_->services(),
    offset_+value::stringLength(measured_->services().normalizeNfc(insertString)));
}
Cursor Cursor::insert(const std::string& value)const{
  return modifyText(*this,value);
}
Cursor Cursor::del()const{
  if(isAtEnd())return *this;
  return modifyText(right());
}
Cursor Cursor::backspace()const{
  if(isAtStart())return *this;
  return left().modifyText(*this);
}
CursorDeleteResult Cursor::deleteToLineStart()const{
  const auto units=value::utf16CodeUnits(text());
  if(offset_>0&&units[offset_-1]==u'\n')
    return {left().modifyText(*this),"\n"};
  const auto start=startOfLine();
  return {start.modifyText(*this),value::slice(text(),start.offset_,offset_)};
}
CursorDeleteResult Cursor::deleteToLineEnd()const{
  const auto units=value::utf16CodeUnits(text());
  if(offset_<units.size()&&units[offset_]==u'\n')
    return {modifyText(right()),"\n"};
  const auto end=endOfLine();
  return {modifyText(end),value::slice(text(),offset_,end.offset_)};
}
Cursor Cursor::deleteToLogicalLineEnd()const{
  const auto units=value::utf16CodeUnits(text());
  if(offset_<units.size()&&units[offset_]==u'\n')return modifyText(right());
  return modifyText(endOfLogicalLine());
}
CursorDeleteResult Cursor::deleteWordBefore()const{
  if(isAtStart())return {*this,{}};
  const auto target=snapOutOfImageRef(prevWord().offset_,true);
  const Cursor previous(measured_,target);
  return {previous.modifyText(*this),value::slice(text(),target,offset_)};
}
std::optional<Cursor> Cursor::deleteTokenBefore()const{
  if(const auto chip=imageRefStartingAt(offset_)){
    auto end=chip->second;
    const auto units=value::utf16CodeUnits(text());
    if(end<units.size()&&units[end]==u' ')++end;
    return modifyText(Cursor(measured_,end));
  }
  if(isAtStart())return {};
  const auto units=value::utf16CodeUnits(text());
  if(offset_<units.size()&&!isVimWhitespace(value::fromUtf16(
      std::u16string_view(units).substr(offset_,1))))return {};
  const auto before=value::slice(text(),0,offset_);
  static const std::regex token(
    R"((^|\s)\[(Pasted text #\d+(?: \+\d+ lines)?|Image #\d+|\.\.\.Truncated text #\d+ \+\d+ lines\.\.\.)\]$)");
  std::smatch match;
  if(!std::regex_search(before,match,token))return {};
  const auto start=value::stringLength(before.substr(0,
    static_cast<std::size_t>(match.position(0)+match.length(1))));
  return Cursor(measured_,start).modifyText(*this);
}
Cursor Cursor::deleteWordAfter()const{
  if(isAtEnd())return *this;
  const auto target=snapOutOfImageRef(nextWord().offset_,false);
  return modifyText(Cursor(measured_,target));
}
} // namespace batbox::interactive::text_input
