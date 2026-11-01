#include "keyboard_input/Keyboard.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;

namespace {
std::vector<keyboard_input::ParsedInput> feed(const std::string& wire,std::size_t chunk){
  keyboard_input::KeyParseState state;
  std::vector<keyboard_input::ParsedInput> keys;
  for(std::size_t offset=0;offset<wire.size();offset+=chunk){
    auto parsed=keyboard_input::parseMultipleKeypresses(state,
      keyboard_input::Input{Json(wire.substr(offset,chunk))});
    state=std::move(parsed.state);
    keys.insert(keys.end(),parsed.keys.begin(),parsed.keys.end());
  }
  return keys;
}
std::string typed(const std::vector<keyboard_input::ParsedInput>& keys){
  std::string text;
  for(const auto& item:keys)
    if(const auto key=std::get_if<keyboard_input::KeyRef>(&item);key&&(*key)->sequence)
      text+=*(*key)->sequence;
  return text;
}
}

int main(){
  // tmux, iTerm2 and xterm answer FTXUI's startup cursor-shape query with a
  // DECRQSS reply. It must reach the response handler, not the prompt.
  const std::string reply="\x1bP1$r q0 q\x1b\\";
  for(const std::size_t chunk:{1,3,64}){
    const auto keys=feed(reply+"hi",chunk);
    bool answered=false;
    for(const auto& item:keys)
      if(const auto response=std::get_if<keyboard_input::ParsedResponse>(&item)){
        assert((*response->response)["type"]=="decrqss");
        assert((*response->response)["valid"]==true);
        assert((*response->response)["data"]==" q0 q");
        answered=true;
      }
    assert(answered);
    assert(typed(keys)=="hi");
  }
  const auto invalid=keyboard_input::parseTerminalResponse("\x1bP0$r\x1b\\");
  assert(invalid&&(*invalid)["type"]=="decrqss"&&(*invalid)["valid"]==false);
  const auto other=keyboard_input::parseTerminalResponse("\x1bP+r544e\x1b\\");
  assert(other&&(*other)["type"]=="dcs");
  const auto version=keyboard_input::parseTerminalResponse("\x1bP>|tmux 3.5\x1b\\");
  assert(version&&(*version)["type"]=="xtversion"&&(*version)["name"]=="tmux 3.5");
  std::cout<<"Terminal control-string replies never become typed text\n";
}
