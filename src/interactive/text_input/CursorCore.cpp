#include "interactive/text_input/Cursor.hpp"
#include "runtime/JsonValue.hpp"
#include "history/References.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::text_input {
Cursor::Cursor(std::shared_ptr<MeasuredText> text,std::size_t offset,
               std::size_t selection):measured_(std::move(text)),
    offset_(offset),selection_(selection){
  if(!measured_)throw std::invalid_argument("Cursor measured text required");
  offset_=std::min(offset_,measured_->length());
  offset_=snapOutOfReference(offset_,true);
}
Cursor Cursor::fromText(std::string text,double columns,
                        MetricsServices services,std::size_t offset,
                        std::size_t selection){
  return Cursor(std::make_shared<MeasuredText>(std::move(text),columns-1,
    std::move(services)),offset,selection);
}
Position Cursor::getPosition()const{
  return measured_->getPositionFromOffset(offset_);
}
std::size_t Cursor::getOffset(Position position)const{
  const auto target=measured_->getOffsetFromPosition(position);
  return snapOutOfReference(target,target<offset_);
}
std::size_t Cursor::getViewportStartLine(
    std::optional<std::size_t> maxVisibleLines)const{
  if(!maxVisibleLines||*maxVisibleLines==0)return 0;
  const auto line=getPosition().line;
  const auto allLines=measured_->getWrappedText();
  if(allLines.size()<=*maxVisibleLines)return 0;
  const auto half=*maxVisibleLines/2;
  auto start=std::max<std::ptrdiff_t>(0,
    line-static_cast<std::ptrdiff_t>(half));
  const auto end=std::min<std::size_t>(allLines.size(),
    static_cast<std::size_t>(start)+*maxVisibleLines);
  if(end-static_cast<std::size_t>(start)<*maxVisibleLines)
    start=std::max<std::ptrdiff_t>(0,
      static_cast<std::ptrdiff_t>(end)-
      static_cast<std::ptrdiff_t>(*maxVisibleLines));
  return static_cast<std::size_t>(start);
}
std::size_t Cursor::getViewportCharOffset(
    std::optional<std::size_t> maxVisibleLines)const{
  const auto start=getViewportStartLine(maxVisibleLines);
  if(start==0)return 0;
  const auto& lines=measured_->getWrappedLines();
  return start<lines.size()?lines[start].startOffset:0;
}
std::size_t Cursor::getViewportCharEnd(
    std::optional<std::size_t> maxVisibleLines)const{
  const auto start=getViewportStartLine(maxVisibleLines);
  const auto& lines=measured_->getWrappedLines();
  if(!maxVisibleLines||*maxVisibleLines==0)return measured_->length();
  const auto end=std::min(lines.size(),start+*maxVisibleLines);
  return end>=lines.size()?measured_->length():lines[end].startOffset;
}
std::optional<std::pair<std::size_t,std::size_t>>
Cursor::referenceStartingAt(std::size_t offset)const{
  for(const auto& ref:history::parseReferences(text()))
    if(ref.index==offset)return {{offset,offset+value::stringLength(ref.match)}};
  return {};
}
std::optional<std::pair<std::size_t,std::size_t>>
Cursor::referenceEndingAt(std::size_t offset)const{
  for(const auto& ref:history::parseReferences(text()))
    if(ref.index+value::stringLength(ref.match)==offset)
      return {{ref.index,offset}};
  return {};
}
std::size_t Cursor::snapOutOfReference(std::size_t offset,
                                       bool towardStart)const{
  for(const auto& ref:history::parseReferences(text())){
    const auto end=ref.index+value::stringLength(ref.match);
    if(offset>ref.index&&offset<end)return towardStart?ref.index:end;
  }
  return offset;
}
Cursor Cursor::left()const{
  if(offset_==0)return *this;
  if(const auto chip=referenceEndingAt(offset_))
    return Cursor(measured_,chip->first);
  return Cursor(measured_,measured_->prevOffset(offset_));
}
Cursor Cursor::right()const{
  if(isAtEnd())return *this;
  if(const auto chip=referenceStartingAt(offset_))
    return Cursor(measured_,chip->second);
  return Cursor(measured_,std::min(measured_->nextOffset(offset_),
                                   measured_->length()));
}
Cursor Cursor::up()const{
  const auto [line,column]=getPosition();
  if(line==0)return *this;
  const auto lines=measured_->getWrappedText();
  if(static_cast<std::size_t>(line-1)>=lines.size())return *this;
  const auto width=measured_->width(lines[static_cast<std::size_t>(line-1)]);
  return Cursor(measured_,getOffset({line-1,std::min(column,width)}),0);
}
Cursor Cursor::down()const{
  const auto [line,column]=getPosition();
  if(line>=static_cast<std::ptrdiff_t>(measured_->lineCount())-1)return *this;
  const auto lines=measured_->getWrappedText();
  if(static_cast<std::size_t>(line+1)>=lines.size())return *this;
  const auto width=measured_->width(lines[static_cast<std::size_t>(line+1)]);
  return Cursor(measured_,getOffset({line+1,std::min(column,width)}),0);
}
Cursor Cursor::startOfLine()const{
  const auto [line,column]=getPosition();
  if(column==0&&line>0)
    return Cursor(measured_,getOffset({line-1,0}),0);
  return Cursor(measured_,getOffset({line,0}),0);
}
Cursor Cursor::firstNonBlankInLine()const{
  // This navigation operation currently selects column zero;
  // it does not skip leading whitespace.
  const auto line=getPosition().line;
  return Cursor(measured_,getOffset({line,0}),0);
}
Cursor Cursor::endOfLine()const{
  const auto line=getPosition().line;
  return Cursor(measured_,getOffset({line,measured_->getLineLength(line)}),0);
}
std::size_t Cursor::findLogicalLineStart(std::size_t fromOffset)const{
  const auto units=value::utf16CodeUnits(text());
  if(fromOffset==0)return 0;
  const auto found=units.rfind(u'\n',fromOffset-1);
  return found==std::u16string::npos?0:found+1;
}
std::size_t Cursor::findLogicalLineEnd(std::size_t fromOffset)const{
  const auto units=value::utf16CodeUnits(text());
  const auto found=units.find(u'\n',fromOffset);
  return found==std::u16string::npos?measured_->length():found;
}
std::pair<std::size_t,std::size_t> Cursor::logicalLineBounds()const{
  return {findLogicalLineStart(offset_),findLogicalLineEnd(offset_)};
}
Cursor Cursor::createCursorWithColumn(std::size_t start,std::size_t end,
                                       std::size_t column)const{
  const auto clamped=std::min(column,end-start);
  const auto target=measured_->snapToGraphemeBoundary(start+clamped);
  return Cursor(measured_,snapOutOfReference(target,target<offset_),0);
}
Cursor Cursor::endOfLogicalLine()const{
  return Cursor(measured_,findLogicalLineEnd(offset_),0);
}
Cursor Cursor::startOfLogicalLine()const{
  return Cursor(measured_,findLogicalLineStart(offset_),0);
}
Cursor Cursor::firstNonBlankInLogicalLine()const{
  const auto [start,end]=logicalLineBounds();
  const auto units=value::utf16CodeUnits(value::slice(text(),start,end));
  std::size_t first=0;
  while(first<units.size()&&isVimWhitespace(value::fromUtf16(
    std::u16string_view(units).substr(first,1))))++first;
  return Cursor(measured_,start+(first<units.size()?first:0),0);
}
Cursor Cursor::upLogicalLine()const{
  const auto [start,end]=logicalLineBounds();
  (void)end;
  if(start==0)return Cursor(measured_,0,0);
  const auto column=offset_-start;
  const auto prevEnd=start-1;
  return createCursorWithColumn(findLogicalLineStart(prevEnd),prevEnd,column);
}
Cursor Cursor::downLogicalLine()const{
  const auto [start,end]=logicalLineBounds();
  if(end>=measured_->length())return Cursor(measured_,measured_->length(),0);
  const auto column=offset_-start;
  const auto nextStart=end+1;
  return createCursorWithColumn(nextStart,findLogicalLineEnd(nextStart),column);
}
Cursor Cursor::startOfFirstLine()const{return Cursor(measured_,0,0);}
Cursor Cursor::startOfLastLine()const{
  const auto units=value::utf16CodeUnits(text());
  const auto found=units.rfind(u'\n');
  if(found==std::u16string::npos)return startOfLine();
  return Cursor(measured_,found+1,0);
}
Cursor Cursor::goToLine(long long lineNumber)const{
  const auto units=value::utf16CodeUnits(text());
  const auto target=std::max(0LL,lineNumber-1);
  long long current=0;
  std::size_t offset=0;
  while(current<target&&offset<units.size()){
    const auto found=units.find(u'\n',offset);
    if(found==std::u16string::npos)break;
    offset=found+1;++current;
  }
  return Cursor(measured_,offset,0);
}
Cursor Cursor::endOfFile()const{
  return Cursor(measured_,measured_->length(),0);
}
bool Cursor::equals(const Cursor& other)const{
  return offset_==other.offset_&&measured_==other.measured_;
}
std::string Cursor::graphemeAt(std::size_t position)const{
  if(position>=measured_->length())return {};
  return value::slice(text(),position,measured_->nextOffset(position));
}
bool Cursor::isOverWhitespace()const{
  return isVimWhitespace(value::substring(text(),offset_,offset_+1));
}
std::optional<std::size_t> Cursor::findCharacter(const std::string& ch,
    char type,std::size_t count)const{
  if(type!='f'&&type!='F'&&type!='t'&&type!='T')
    throw std::invalid_argument("Vim find type required");
  const bool forward=type=='f'||type=='t';
  const bool till=type=='t'||type=='T';
  std::size_t found=0;
  if(forward){
    for(auto position=measured_->nextOffset(offset_);
        position<measured_->length();
        position=measured_->nextOffset(position)){
      if(graphemeAt(position)==ch&&++found==count)
        return till?std::max(offset_,measured_->prevOffset(position)):position;
    }
  }else{
    if(offset_==0)return {};
    auto position=measured_->prevOffset(offset_);
    for(;;){
      if(graphemeAt(position)==ch&&++found==count)
        return till?std::min(offset_,measured_->nextOffset(position)):position;
      if(position==0)break;
      position=measured_->prevOffset(position);
    }
  }
  return {};
}
} // namespace batbox::interactive::text_input
