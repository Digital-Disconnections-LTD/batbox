#include "interactive/text_input/TextEditor.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::text_input {
TextEditor::TextEditor(event_loop::Loop& owner,KillRing& killRing,
    MetricsServices metrics,EditorOptions options,EditorCallbacks callbacks):
    owner_(owner),killRing_(killRing),metrics_(std::move(metrics)),
    options_(std::move(options)),callbacks_(std::move(callbacks)){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Text editor requires owner thread");
  if(!callbacks_.onChange||!callbacks_.onOffsetChange||
     !callbacks_.stripAnsi||!callbacks_.invert||!callbacks_.nowMs)
    throw std::invalid_argument("Text editor imports required");
  if(options_.appleTerminal&&callbacks_.prewarmModifiers)
    callbacks_.prewarmModifiers();
}
TextEditor::~TextEditor(){
  ctrlC_.timeout.cancel();escape_.timeout.cancel();ctrlD_.timeout.cancel();
}
void TextEditor::setOptions(EditorOptions options){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Editor option update requires owner thread");
  options_=std::move(options);
}
void TextEditor::change(interactive::InputState& state,
                        const std::string& text){
  state.text=text;
  callbacks_.onChange(text);
}
void TextEditor::setOffset(interactive::InputState& state,
                           std::size_t offset){
  state.caretUtf16=offset;
  callbacks_.onOffsetChange(offset);
}
void TextEditor::press(DoublePress& tracker,
    const std::function<void(bool)>& pending,
    const std::function<void()>& doubled,
    const std::function<void()>& first){
  const double now=callbacks_.nowMs();
  const bool twice=now-tracker.lastPress<=800&&tracker.timeout.valid();
  if(twice){
    tracker.timeout.cancel();tracker.timeout={};
    pending(false);
    doubled();
  }else{
    if(first)first();
    pending(true);
    tracker.timeout.cancel();
    auto* current=&tracker;
    tracker.timeout=owner_.setTimeout(800,[current,pending]{
      current->timeout={};
      pending(false);
    });
  }
  tracker.lastPress=now;
}
void TextEditor::onParsedKey(const terminal_events::ParsedKey& key,
                             interactive::InputState& state){
  const auto parsed=terminal_events::parseInputKey(key);
  onInput(parsed.second,parsed.first,state);
}
void TextEditor::onInput(const std::string& input,
                         const terminal_events::Key& key,
                         interactive::InputState& state){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Text input requires owner thread");
  const auto original=state.text;
  const auto filtered=callbacks_.inputFilter?
    callbacks_.inputFilter(input,key):input;
  if(filtered.empty()&&!input.empty())return;
  const auto cursor=Cursor::fromText(original,options_.columns,metrics_,
                                     state.caretUtf16);
  if(!key.backspace&&!key.deleteKey&&input.find('\x7f')!=std::string::npos){
    auto current=cursor;
    for(const auto character:input){
      if(character!='\x7f')continue;
      const auto token=current.deleteTokenBefore();
      current=token?*token:current.backspace();
    }
    if(!cursor.equals(current)){
      if(cursor.text()!=current.text())change(state,current.text());
      setOffset(state,current.offset());
    }
    killRing_.resetKillAccumulation();killRing_.resetYankState();
    return;
  }
  const bool kill=(key.ctrl&&(filtered=="k"||filtered=="u"||filtered=="w"))||
    (key.meta&&(key.backspace||key.deleteKey));
  const bool yank=(key.ctrl||key.meta)&&filtered=="y";
  if(!kill)killRing_.resetKillAccumulation();
  if(!yank)killRing_.resetYankState();
  const auto next=mapKey(filtered,key,cursor,state);
  if(!next)return;
  if(!cursor.equals(*next)){
    if(cursor.text()!=next->text())change(state,next->text());
    setOffset(state,next->offset());
  }
  const auto units=value::utf16CodeUnits(filtered);
  if(units.size()>1&&units.back()==u'\r'&&
     units[units.size()-2]!=u'\\'&&
     std::find(units.begin(),units.end()-1,u'\r')==units.end()-1&&
     callbacks_.onSubmit)
    callbacks_.onSubmit(next->text());
}
EditorView TextEditor::view(const interactive::InputState& state,
                            const std::optional<InlineGhostText>& ghost,
                            const std::function<std::string(const std::string&)>& dim)const{
  const auto cursor=Cursor::fromText(state.text,options_.columns,metrics_,
                                     state.caretUtf16);
  const auto position=cursor.getPosition();
  const auto start=cursor.getViewportStartLine(options_.maxVisibleLines);
  std::optional<GhostText> shown;
  if(ghost&&dim&&ghost->insertPosition==state.caretUtf16)
    shown=GhostText{ghost->text,dim};
  return {cursor.render(options_.cursorChar,options_.mask,
      callbacks_.invert,shown,options_.maxVisibleLines),cursor.offset(),
    cursor.getViewportCharOffset(options_.maxVisibleLines),
    cursor.getViewportCharEnd(options_.maxVisibleLines),
    position.line-static_cast<std::ptrdiff_t>(start),position.column};
}
} // namespace batbox::interactive::text_input
