#include "keyboard_input/Keyboard.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;

int main(){
  std::string payload="SUMMARY\r\n";
  for(int i=0;i<70;++i)payload+="Systems — café, 雪 and 🦇; migrations and documentation.\r\n";
  const auto wire="\x1b[200~"+payload+"\x1b[201~";
  // Terminal reads can end at any byte, including inside a Unicode scalar
  // or either bracketed-paste delimiter.
  for(const std::size_t chunk:{1,2,3,37,1024,4096}){
    keyboard_input::KeyParseState state;
    std::vector<keyboard_input::ParsedInput> keys;
    for(std::size_t offset=0;offset<wire.size();offset+=chunk){
      auto parsed=keyboard_input::parseMultipleKeypresses(state,
        keyboard_input::Input{Json(wire.substr(offset,chunk))});
      state=std::move(parsed.state);
      keys.insert(keys.end(),parsed.keys.begin(),parsed.keys.end());
    }
    assert(keys.size()==1);
    const auto key=std::get<keyboard_input::KeyRef>(keys.front());
    assert(key->isPasted&&key->sequence==payload);
    assert(state.mode=="NORMAL"&&state.pasteBuffer.empty());
  }
  // Ordinary typing uses the same streaming decoder.
  termio::Tokenizer tokenizer;
  assert(tokenizer.feed("\xe2").empty());
  assert(tokenizer.feed("\x80").empty());
  auto tokens=tokenizer.feed("\x94");
  assert(tokens.size()==1&&tokens[0].value=="—");
  assert(tokenizer.feed("\xf0\x9f").empty());
  tokenizer.reset();
  tokens=tokenizer.feed("ok");
  assert(tokens.size()==1&&tokens[0].value=="ok");
  std::cout<<"Unicode paste survives every terminal read boundary\n";
}
