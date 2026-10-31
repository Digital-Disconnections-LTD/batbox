#pragma once

#include "interactive/text_input/MeasuredText.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace batbox::interactive::text_input {

bool isVimWordChar(const std::string& grapheme);
bool isVimWhitespace(const std::string& grapheme);
bool isVimPunctuation(const std::string& grapheme);

struct CursorDeleteResult;
struct GhostText {
  std::string text;
  std::function<std::string(const std::string&)> dim;
};
class Cursor {
public:
  Cursor(std::shared_ptr<MeasuredText> text,std::size_t offset=0,
         std::size_t selection=0);
  static Cursor fromText(std::string text,double columns,
                         MetricsServices services,std::size_t offset=0,
                         std::size_t selection=0);
  const std::string& text()const{return measured_->text();}
  std::size_t offset()const{return offset_;}
  std::size_t selection()const{return selection_;}
  double columns()const{return measured_->columns()+1;}
  std::shared_ptr<MeasuredText> measuredText()const{return measured_;}
  Position getPosition()const;
  std::size_t getViewportStartLine(
      std::optional<std::size_t> maxVisibleLines={})const;
  std::size_t getViewportCharOffset(
      std::optional<std::size_t> maxVisibleLines={})const;
  std::size_t getViewportCharEnd(
      std::optional<std::size_t> maxVisibleLines={})const;
  std::string render(const std::string& cursorChar,const std::string& mask,
      const std::function<std::string(const std::string&)>& invert,
      const std::optional<GhostText>& ghostText={},
      std::optional<std::size_t> maxVisibleLines={})const;
  Cursor left()const;
  Cursor right()const;
  std::optional<std::pair<std::size_t,std::size_t>> imageRefEndingAt(
      std::size_t offset)const;
  std::optional<std::pair<std::size_t,std::size_t>> imageRefStartingAt(
      std::size_t offset)const;
  std::size_t snapOutOfImageRef(std::size_t offset,bool towardStart)const;
  Cursor up()const;
  Cursor down()const;
  Cursor startOfLine()const;
  Cursor firstNonBlankInLine()const;
  Cursor endOfLine()const;
  Cursor endOfLogicalLine()const;
  Cursor startOfLogicalLine()const;
  Cursor firstNonBlankInLogicalLine()const;
  Cursor upLogicalLine()const;
  Cursor downLogicalLine()const;
  Cursor nextWord()const;
  Cursor endOfWord()const;
  Cursor prevWord()const;
  Cursor nextVimWord()const;
  Cursor endOfVimWord()const;
  Cursor prevVimWord()const;
  Cursor nextWORD()const;
  Cursor endOfWORD()const;
  Cursor prevWORD()const;
  Cursor modifyText(const Cursor& end,
                    const std::string& insertString="")const;
  Cursor insert(const std::string& text)const;
  Cursor del()const;
  Cursor backspace()const;
  CursorDeleteResult deleteToLineStart()const;
  CursorDeleteResult deleteToLineEnd()const;
  Cursor deleteToLogicalLineEnd()const;
  CursorDeleteResult deleteWordBefore()const;
  std::optional<Cursor> deleteTokenBefore()const;
  Cursor deleteWordAfter()const;
  bool equals(const Cursor& other)const;
  bool isAtStart()const{return offset_==0;}
  bool isAtEnd()const{return offset_>=measured_->length();}
  Cursor startOfFirstLine()const;
  Cursor startOfLastLine()const;
  Cursor goToLine(long long lineNumber)const;
  Cursor endOfFile()const;
  std::optional<std::size_t> findCharacter(const std::string& ch,
      char type,std::size_t count=1)const;
private:
  std::size_t findLogicalLineStart(std::size_t fromOffset)const;
  std::size_t findLogicalLineEnd(std::size_t fromOffset)const;
  std::pair<std::size_t,std::size_t> logicalLineBounds()const;
  Cursor createCursorWithColumn(std::size_t lineStart,std::size_t lineEnd,
                                 std::size_t targetColumn)const;
  std::size_t getOffset(Position position)const;
  std::string graphemeAt(std::size_t position)const;
  bool isOverWhitespace()const;
  std::shared_ptr<MeasuredText> measured_;
  std::size_t offset_,selection_;
};
struct CursorDeleteResult {Cursor cursor;std::string killed;};

} // namespace batbox::interactive::text_input
