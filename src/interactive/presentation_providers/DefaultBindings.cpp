#include "interactive/presentation_providers/DefaultBindings.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace batbox::interactive::presentation_providers {
namespace {
std::vector<BindingBlock> baseBlocks(){
  return {
    {"Global",{
      {"ctrl+c","app:interrupt"},
      {"ctrl+d","app:exit"},
      {"ctrl+l","app:redraw"},
      {"ctrl+t","app:toggleTodos"},
      {"ctrl+o","app:toggleTranscript"},
      {"ctrl+shift+o","app:toggleTeammatePreview"},
      {"ctrl+r","history:search"},
    }},
    {"Chat",{
      {"escape","chat:cancel"},
      {"ctrl+x ctrl+k","chat:killAgents"},
      {"shift+tab","chat:cycleMode"},
      {"meta+p","chat:modelPicker"},
      {"meta+o","chat:fastMode"},
      {"meta+t","chat:thinkingToggle"},
      {"enter","chat:submit"},
      {"up","history:previous"},
      {"down","history:next"},
      {"ctrl+_","chat:undo"},
      {"ctrl+shift+-","chat:undo"},
      {"ctrl+x ctrl+e","chat:externalEditor"},
      {"ctrl+g","chat:externalEditor"},
      {"ctrl+s","chat:stash"},
      {"ctrl+v","chat:imagePaste"},
    }},
    {"Autocomplete",{
      {"tab","autocomplete:accept"},
      {"escape","autocomplete:dismiss"},
      {"up","autocomplete:previous"},
      {"down","autocomplete:next"},
    }},
    {"Settings",{
      {"escape","confirm:no"},
      {"up","select:previous"},
      {"down","select:next"},
      {"k","select:previous"},
      {"j","select:next"},
      {"ctrl+p","select:previous"},
      {"ctrl+n","select:next"},
      {"space","select:accept"},
      {"enter","settings:close"},
      {"/","settings:search"},
      {"r","settings:retry"},
    }},
    {"Confirmation",{
      {"y","confirm:yes"},
      {"n","confirm:no"},
      {"enter","confirm:yes"},
      {"escape","confirm:no"},
      {"up","confirm:previous"},
      {"down","confirm:next"},
      {"tab","confirm:nextField"},
      {"space","confirm:toggle"},
      {"shift+tab","confirm:cycleMode"},
      {"ctrl+e","confirm:toggleExplanation"},
      {"ctrl+d","permission:toggleDebug"},
    }},
    {"Tabs",{
      {"tab","tabs:next"},
      {"shift+tab","tabs:previous"},
      {"right","tabs:next"},
      {"left","tabs:previous"},
    }},
    {"Transcript",{
      {"ctrl+e","transcript:toggleShowAll"},
      {"ctrl+c","transcript:exit"},
      {"escape","transcript:exit"},
      {"q","transcript:exit"},
    }},
    {"HistorySearch",{
      {"ctrl+r","historySearch:next"},
      {"escape","historySearch:accept"},
      {"tab","historySearch:accept"},
      {"ctrl+c","historySearch:cancel"},
      {"enter","historySearch:execute"},
    }},
    {"Task",{
      {"ctrl+b","task:background"},
    }},
    {"ThemePicker",{
      {"ctrl+t","theme:toggleSyntaxHighlighting"},
    }},
    {"Scroll",{
      {"pageup","scroll:pageUp"},
      {"pagedown","scroll:pageDown"},
      {"wheelup","scroll:lineUp"},
      {"wheeldown","scroll:lineDown"},
      {"ctrl+home","scroll:top"},
      {"ctrl+end","scroll:bottom"},
      {"ctrl+shift+c","selection:copy"},
      {"cmd+c","selection:copy"},
    }},
    {"Help",{
      {"escape","help:dismiss"},
    }},
    {"Attachments",{
      {"right","attachments:next"},
      {"left","attachments:previous"},
      {"backspace","attachments:remove"},
      {"delete","attachments:remove"},
      {"down","attachments:exit"},
      {"escape","attachments:exit"},
    }},
    {"Footer",{
      {"up","footer:up"},
      {"ctrl+p","footer:up"},
      {"down","footer:down"},
      {"ctrl+n","footer:down"},
      {"right","footer:next"},
      {"left","footer:previous"},
      {"enter","footer:openSelected"},
      {"escape","footer:clearSelection"},
    }},
    {"MessageSelector",{
      {"up","messageSelector:up"},
      {"down","messageSelector:down"},
      {"k","messageSelector:up"},
      {"j","messageSelector:down"},
      {"ctrl+p","messageSelector:up"},
      {"ctrl+n","messageSelector:down"},
      {"ctrl+up","messageSelector:top"},
      {"shift+up","messageSelector:top"},
      {"meta+up","messageSelector:top"},
      {"shift+k","messageSelector:top"},
      {"ctrl+down","messageSelector:bottom"},
      {"shift+down","messageSelector:bottom"},
      {"meta+down","messageSelector:bottom"},
      {"shift+j","messageSelector:bottom"},
      {"enter","messageSelector:select"},
    }},
    {"DiffDialog",{
      {"escape","diff:dismiss"},
      {"left","diff:previousSource"},
      {"right","diff:nextSource"},
      {"up","diff:previousFile"},
      {"down","diff:nextFile"},
      {"enter","diff:viewDetails"},
    }},
    {"ModelPicker",{
      {"left","modelPicker:decreaseEffort"},
      {"right","modelPicker:increaseEffort"},
    }},
    {"Select",{
      {"up","select:previous"},
      {"down","select:next"},
      {"j","select:next"},
      {"k","select:previous"},
      {"ctrl+n","select:next"},
      {"ctrl+p","select:previous"},
      {"enter","select:accept"},
      {"escape","select:cancel"},
    }},
    {"Plugin",{
      {"space","plugin:toggle"},
      {"i","plugin:install"},
    }},
  };
}
BindingBlock messageActionsBlock(){
  return {"MessageActions",{
    {"up","messageActions:prev"},
    {"down","messageActions:next"},
    {"k","messageActions:prev"},
    {"j","messageActions:next"},
    {"meta+up","messageActions:top"},
    {"meta+down","messageActions:bottom"},
    {"super+up","messageActions:top"},
    {"super+down","messageActions:bottom"},
    {"shift+up","messageActions:prevUser"},
    {"shift+down","messageActions:nextUser"},
    {"escape","messageActions:escape"},
    {"ctrl+c","messageActions:ctrlc"},
    {"enter","messageActions:enter"},
    {"c","messageActions:c"},
    {"p","messageActions:p"},
  }};
}
BindingBlock& block(std::vector<BindingBlock>& blocks,const std::string& context){
  auto found=std::find_if(blocks.begin(),blocks.end(),
    [&](const auto& item){return item.context==context;});
  if(found==blocks.end())throw std::logic_error("Missing default binding context");
  return *found;
}
void insertBefore(BindingBlock& value,const std::string& before,
    std::string key,std::string action){
  auto found=std::find_if(value.bindings.begin(),value.bindings.end(),
    [&](const auto& item){return item.first==before;});
  value.bindings.insert(found,{std::move(key),std::move(action)});
}
void replaceKey(BindingBlock& value,const std::string& oldKey,
    std::string newKey){
  auto found=std::find_if(value.bindings.begin(),value.bindings.end(),
    [&](const auto& item){return item.first==oldKey;});
  if(found==value.bindings.end())throw std::logic_error("Missing default binding key");
  found->first=std::move(newKey);
}
} // namespace
std::vector<BindingBlock> defaultBindingBlocks(const DefaultBindingOptions& options){
  if(!options.feature)throw std::invalid_argument("Feature selector required");
  auto blocks=baseBlocks();
  auto& global=block(blocks,"Global");
  if(options.feature("KAIROS")||options.feature("KAIROS_BRIEF"))
    insertBefore(global,"ctrl+shift+o","ctrl+shift+b","app:toggleBrief");
  if(options.feature("QUICK_SEARCH")){
    global.bindings.push_back({"ctrl+shift+f","app:globalSearch"});
    global.bindings.push_back({"cmd+shift+f","app:globalSearch"});
    global.bindings.push_back({"ctrl+shift+p","app:quickOpen"});
    global.bindings.push_back({"cmd+shift+p","app:quickOpen"});
  }
  if(options.feature("TERMINAL_PANEL"))
    global.bindings.push_back({"meta+j","app:toggleTerminal"});
  auto& chat=block(blocks,"Chat");
  if(options.platform=="windows"){
    if(!options.terminalVtMode)replaceKey(chat,"shift+tab","meta+m");
    replaceKey(chat,"ctrl+v","alt+v");
  }
  if(options.feature("MESSAGE_ACTIONS"))
    chat.bindings.push_back({"shift+up","chat:messageActions"});
  if(options.feature("VOICE_MODE"))chat.bindings.push_back({"space","voice:pushToTalk"});
  if(options.feature("MESSAGE_ACTIONS")){
    auto found=std::find_if(blocks.begin(),blocks.end(),
      [](const auto& item){return item.context=="MessageSelector";});
    blocks.insert(found+1,messageActionsBlock());
  }
  return blocks;
}
std::vector<Binding> defaultBindings(const DefaultBindingOptions& options){
  return parseBindings(defaultBindingBlocks(options));
}
ShortcutProvider makeShortcutProvider(DefaultBindingOptions options,
    std::function<std::vector<Binding>()> loadUserBindings){
  if(!loadUserBindings)
    throw std::invalid_argument("User keybinding loader required");
  return ShortcutProvider{[options=std::move(options),
      loadUserBindings=std::move(loadUserBindings)]{
    auto bindings=defaultBindings(options);
    auto user=loadUserBindings();
    bindings.insert(bindings.end(),std::make_move_iterator(user.begin()),
      std::make_move_iterator(user.end()));
    return bindings;
  }};
}
} // namespace batbox::interactive::presentation_providers
