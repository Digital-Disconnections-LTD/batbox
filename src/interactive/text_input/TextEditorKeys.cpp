#include "interactive/text_input/TextEditor.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>

namespace batbox::interactive::text_input {

Cursor TextEditor::killToLineEnd(const Cursor& cursor){
  auto result=cursor.deleteToLineEnd();
  killRing_.push(result.killed,KillRing::Direction::Append);
  return result.cursor;
}
Cursor TextEditor::killToLineStart(const Cursor& cursor){
  auto result=cursor.deleteToLineStart();
  killRing_.push(result.killed,KillRing::Direction::Prepend);
  return result.cursor;
}
Cursor TextEditor::killWordBefore(const Cursor& cursor){
  auto result=cursor.deleteWordBefore();
  killRing_.push(result.killed,KillRing::Direction::Prepend);
  return result.cursor;
}
Cursor TextEditor::yank(const Cursor& cursor){
  const auto& text=killRing_.last();
  if(text.empty())return cursor;
  auto next=cursor.insert(text);
  killRing_.recordYank(cursor.offset(),value::stringLength(text));
  return next;
}
Cursor TextEditor::yankPop(const Cursor& cursor){
  const auto result=killRing_.yankPop();
  if(!result)return cursor;
  const auto text=value::slice(cursor.text(),0,result->start)+result->text+
    value::slice(cursor.text(),result->start+result->length);
  const auto offset=result->start+value::stringLength(result->text);
  killRing_.updateYankLength(value::stringLength(result->text));
  return Cursor::fromText(text,options_.columns,metrics_,offset);
}

Cursor TextEditor::upOrHistoryUp(const Cursor& cursor){
  if(options_.disableCursorMovementForUpDownKeys){
    if(callbacks_.onHistoryUp)callbacks_.onHistoryUp();
    return cursor;
  }
  auto next=cursor.up();
  if(!next.equals(cursor))return next;
  if(options_.multiline){
    next=cursor.upLogicalLine();
    if(!next.equals(cursor))return next;
  }
  if(callbacks_.onHistoryUp)callbacks_.onHistoryUp();
  return cursor;
}
Cursor TextEditor::downOrHistoryDown(const Cursor& cursor){
  if(options_.disableCursorMovementForUpDownKeys){
    if(callbacks_.onHistoryDown)callbacks_.onHistoryDown();
    return cursor;
  }
  auto next=cursor.down();
  if(!next.equals(cursor))return next;
  if(options_.multiline){
    next=cursor.downLogicalLine();
    if(!next.equals(cursor))return next;
  }
  if(callbacks_.onHistoryDown)callbacks_.onHistoryDown();
  return cursor;
}

std::optional<Cursor> TextEditor::handleEnter(
    const terminal_events::Key& key,const Cursor& cursor,
    const interactive::InputState& state){
  const auto units=value::utf16CodeUnits(cursor.text());
  if(options_.multiline&&cursor.offset()>0&&
     units[cursor.offset()-1]==u'\\'){
    if(callbacks_.markBackslashReturnUsed)
      callbacks_.markBackslashReturnUsed();
    return cursor.backspace().insert("\n");
  }
  if(key.meta||key.shift)return cursor.insert("\n");
  if(options_.appleTerminal&&callbacks_.isShiftPressed&&
     callbacks_.isShiftPressed())return cursor.insert("\n");
  if(callbacks_.onSubmit)callbacks_.onSubmit(state.text);
  return std::nullopt;
}

std::optional<Cursor> TextEditor::handleCtrl(
    const std::string& input,const Cursor& cursor,
    interactive::InputState& state){
  if(input=="a")return cursor.startOfLine();
  if(input=="b")return cursor.left();
  if(input=="c"){
    press(ctrlC_,[this](bool show){
      if(callbacks_.onExitMessage)callbacks_.onExitMessage(show,"Ctrl-C");
    },[this]{if(callbacks_.onExit)callbacks_.onExit();},
    [this,&state]{
      if(!state.text.empty()){
        change(state,"");setOffset(state,0);
        if(callbacks_.onHistoryReset)callbacks_.onHistoryReset();
      }
    });
    return std::nullopt;
  }
  if(input=="d"){
    if(cursor.text().empty()){
      press(ctrlD_,[this,&state](bool show){
        if(state.text.empty()&&callbacks_.onExitMessage)
          callbacks_.onExitMessage(show,"Ctrl-D");
      },[this,&state]{
        if(state.text.empty()&&callbacks_.onExit)callbacks_.onExit();
      });
      return cursor;
    }
    return cursor.del();
  }
  if(input=="e")return cursor.endOfLine();
  if(input=="f")return cursor.right();
  if(input=="h"){
    auto token=cursor.deleteTokenBefore();
    return token?*token:cursor.backspace();
  }
  if(input=="k")return killToLineEnd(cursor);
  if(input=="n")return downOrHistoryDown(cursor);
  if(input=="p")return upOrHistoryUp(cursor);
  if(input=="u")return killToLineStart(cursor);
  if(input=="w")return killWordBefore(cursor);
  if(input=="y")return yank(cursor);
  return std::nullopt;
}
std::optional<Cursor> TextEditor::handleMeta(
    const std::string& input,const Cursor& cursor){
  if(input=="b")return cursor.prevWord();
  if(input=="f")return cursor.nextWord();
  if(input=="d")return cursor.deleteWordAfter();
  if(input=="y")return yankPop(cursor);
  return std::nullopt;
}

std::optional<Cursor> TextEditor::mapKey(
    const std::string& input,const terminal_events::Key& key,
    const Cursor& cursor,interactive::InputState& state){
  if(key.escape){
    if(options_.disableEscapeDoublePress)return cursor;
    press(escape_,[this,&state](bool show){
      if(state.text.empty()||!show)return;
      if(callbacks_.addNotification)
        callbacks_.addNotification({"escape-again-to-clear",
          "Esc again to clear","immediate",1000});
    },[this,&state]{
      if(callbacks_.removeNotification)
        callbacks_.removeNotification("escape-again-to-clear");
      if(callbacks_.onClearInput)callbacks_.onClearInput();
      if(!state.text.empty()){
        if(!value::trim(state.text).empty()&&callbacks_.addToHistory)
          callbacks_.addToHistory(state.text);
        change(state,"");setOffset(state,0);
        if(callbacks_.onHistoryReset)callbacks_.onHistoryReset();
      }
    });
    return cursor;
  }
  if(key.leftArrow&&(key.ctrl||key.meta||key.fn))return cursor.prevWord();
  if(key.rightArrow&&(key.ctrl||key.meta||key.fn))return cursor.nextWord();
  if(key.backspace){
    if(key.meta||key.ctrl)return killWordBefore(cursor);
    auto token=cursor.deleteTokenBefore();
    return token?*token:cursor.backspace();
  }
  if(key.deleteKey)return key.meta?killToLineEnd(cursor):cursor.del();
  if(key.ctrl)return handleCtrl(input,cursor,state);
  if(key.home)return cursor.startOfLine();
  if(key.end)return cursor.endOfLine();
  if(key.pageDown)return options_.fullscreen?std::nullopt:
    std::optional<Cursor>(cursor.endOfLine());
  if(key.pageUp)return options_.fullscreen?std::nullopt:
    std::optional<Cursor>(cursor.startOfLine());
  if(key.wheelUp||key.wheelDown)return std::nullopt;
  if(key.returnKey)return handleEnter(key,cursor,state);
  if(key.meta)return handleMeta(input,cursor);
  if(key.tab)return cursor;
  if(key.upArrow&&!key.shift)return upOrHistoryUp(cursor);
  if(key.downArrow&&!key.shift)return downOrHistoryDown(cursor);
  if(key.leftArrow)return cursor.left();
  if(key.rightArrow)return cursor.right();
  if(input=="\x1b[H"||input=="\x1b[1~")return cursor.startOfLine();
  if(input=="\x1b[F"||input=="\x1b[4~")return cursor.endOfLine();

  auto text=callbacks_.stripAnsi(input);
  const auto units=value::utf16CodeUnits(text);
  if(units.size()>1&&units.back()==u'\r'&&
     units[units.size()-2]!=u'\\'&&units[units.size()-2]!=u'\r'&&
     units[units.size()-2]!=u'\n')text=value::fromUtf16(
       std::u16string_view(units).substr(0,units.size()-1));
  std::replace(text.begin(),text.end(),'\r','\n');
  if(cursor.isAtStart()&&input=="!")return cursor.insert(text).left();
  return cursor.insert(text);
}
} // namespace batbox::interactive::text_input
