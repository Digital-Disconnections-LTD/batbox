#include "keyboard_input/Keyboard.hpp"
#include "keyboard_input/Regex.hpp"
#include "termio/Csi.hpp"
namespace batbox::keyboard_input {
namespace {
// FTXUI passes one raw terminal read at a time. A read may contain several
// control keys, while this tokenizer emits contiguous C0 bytes as one
// text token. Keep ordinary text (including coalesced Enter and unbracketed
// multiline paste) together, but dispatch each discrete control key.
bool discreteControl(unsigned char byte){
  return byte>=0x01&&byte<=0x1f&&byte!='\n'&&byte!='\r'&&byte!=0x1b;
}
void appendTextKeys(std::vector<ParsedInput>& keys,const std::string& text){
  std::size_t start=0;
  for(std::size_t i=0;i<text.size();++i){
    if(!discreteControl(static_cast<unsigned char>(text[i])))continue;
    if(i>start)keys.push_back(std::make_shared<ParsedKey>(
      parseKeypress(text.substr(start,i-start))));
    keys.push_back(std::make_shared<ParsedKey>(
      parseKeypress(text.substr(i,1))));
    start=i+1;
  }
  if(start<text.size())keys.push_back(std::make_shared<ParsedKey>(
    parseKeypress(text.substr(start))));
}
}
ParseResult parseMultipleKeypresses(const KeyParseState& previous,const Input& input){
  const auto json=std::get_if<Json>(&input);
  const bool isFlush=json&&json->is_null();
  const bool indigenousRawRead=json&&json->is_string();
  const auto inputString=isFlush?"":inputToString(input);
  const auto tokenizer=previous.tokenizer?previous.tokenizer:std::make_shared<termio::Tokenizer>(true);
  const auto tokens=isFlush?tokenizer->flush():tokenizer->feed(inputString);
  ParseResult result;bool inPaste=previous.mode=="IN_PASTE";auto pasteBuffer=previous.pasteBuffer;
  for(const auto& token:tokens){
    if(token.type=="sequence"){
      if(token.value==termio::PASTE_START){inPaste=true;pasteBuffer="";}
      else if(token.value==termio::PASTE_END){result.keys.push_back(std::make_shared<ParsedKey>(createPasteKey(pasteBuffer)));inPaste=false;pasteBuffer="";}
      else if(inPaste)pasteBuffer+=token.value;
      else{
        const auto response=parseTerminalResponse(token.value);
        if(response)result.keys.push_back(ParsedResponse{token.value,std::make_shared<Json>(*response)});
        else{
          const auto mouse=parseMouseEvent(token.value);
          if(mouse)result.keys.push_back(std::make_shared<ParsedMouse>(*mouse));
          else result.keys.push_back(std::make_shared<ParsedKey>(parseKeypress(token.value)));
        }
      }
    }else if(token.type=="text"){
      if(inPaste)pasteBuffer+=token.value;
      else{
        static const glob::Regex16 sgrTail(u"^\\[<\\d+;\\d+;\\d+[Mm]$");
        static const glob::Regex16 x10Tail(u"^\\[M[\x60-\x7f][\x20-\uffff]{2}$");
        Match m;
        if(match(sgrTail,token.value,m)||match(x10Tail,token.value,m)){
          const auto resynthesized="\x1b"+token.value;
          const auto mouse=parseMouseEvent(resynthesized);
          if(mouse)result.keys.push_back(std::make_shared<ParsedMouse>(*mouse));
          else result.keys.push_back(std::make_shared<ParsedKey>(parseKeypress(resynthesized)));
        }else if(indigenousRawRead)appendTextKeys(result.keys,token.value);
        else result.keys.push_back(std::make_shared<ParsedKey>(parseKeypress(token.value)));
      }
    }
  }
  if(isFlush&&inPaste&&!pasteBuffer.empty()){
    result.keys.push_back(std::make_shared<ParsedKey>(createPasteKey(pasteBuffer)));inPaste=false;pasteBuffer="";
  }
  result.state={inPaste?"IN_PASTE":"NORMAL",tokenizer->buffer(),pasteBuffer,tokenizer};
  return result;
}
}
