#include "termio/Tokenizer.hpp"
#include "termio/Csi.hpp"
#include "runtime/JsonValue.hpp"
#include <utility>
namespace batbox::termio {
namespace {
bool isEscFinal(unsigned code){return code>=0x30&&code<=0x7e;}
}
TokenizeResult tokenize(const std::u16string& input,InputState initialState,
  const std::u16string& initialBuffer,bool flush,bool x10Mouse){
  TokenizeResult result{{},initialState,{}};
  const auto data=initialBuffer+input;
  std::size_t i=0,textStart=0,seqStart=0;
  const auto slice=[&](std::size_t start,std::size_t end){
    return value::fromUtf16(std::u16string_view(data).substr(start,end-start));
  };
  const auto flushText=[&]{
    if(i>textStart){auto text=slice(textStart,i);if(!text.empty())result.tokens.push_back({"text",std::move(text)});}
    textStart=i;
  };
  const auto emitSequence=[&](std::string sequence){
    if(!sequence.empty())result.tokens.push_back({"sequence",std::move(sequence)});
    result.state=InputState::ground;textStart=i;
  };
  while(i<data.size()){
    const auto code=data[i];
    switch(result.state){
    case InputState::ground:
      if(code==0x1b){flushText();seqStart=i;result.state=InputState::escape;++i;}
      else ++i;
      break;
    case InputState::escape:
      if(code==0x5b){result.state=InputState::csi;++i;}
      else if(code==0x5d){result.state=InputState::osc;++i;}
      else if(code==0x50){result.state=InputState::dcs;++i;}
      else if(code==0x5f){result.state=InputState::apc;++i;}
      else if(code==0x4f){result.state=InputState::ss3;++i;}
      else if(isCSIIntermediate(code)){result.state=InputState::escapeIntermediate;++i;}
      else if(isEscFinal(code)){++i;emitSequence(slice(seqStart,i));}
      else if(code==0x1b){emitSequence(slice(seqStart,i));seqStart=i;result.state=InputState::escape;++i;}
      else{result.state=InputState::ground;textStart=seqStart;}
      break;
    case InputState::escapeIntermediate:
      if(isCSIIntermediate(code))++i;
      else if(isEscFinal(code)){++i;emitSequence(slice(seqStart,i));}
      else{result.state=InputState::ground;textStart=seqStart;}
      break;
    case InputState::csi:
      if(x10Mouse&&code==0x4d&&i-seqStart==2&&
        (i+1>=data.size()||data[i+1]>=0x20)&&
        (i+2>=data.size()||data[i+2]>=0x20)&&
        (i+3>=data.size()||data[i+3]>=0x20)){
        if(i+4<=data.size()){i+=4;emitSequence(slice(seqStart,i));}
        else i=data.size();
        break;
      }
      if(isCSIFinal(code)){++i;emitSequence(slice(seqStart,i));}
      else if(isCSIParam(code)||isCSIIntermediate(code))++i;
      else{result.state=InputState::ground;textStart=seqStart;}
      break;
    case InputState::ss3:
      if(code>=0x40&&code<=0x7e){++i;emitSequence(slice(seqStart,i));}
      else{result.state=InputState::ground;textStart=seqStart;}
      break;
    case InputState::osc:
    case InputState::dcs:
    case InputState::apc:
      if(code==0x07){++i;emitSequence(slice(seqStart,i));}
      else if(code==0x1b&&i+1<data.size()&&data[i+1]==0x5c){i+=2;emitSequence(slice(seqStart,i));}
      else ++i;
      break;
    }
  }
  if(result.state==InputState::ground)flushText();
  else if(flush){
    auto remaining=slice(seqStart,data.size());
    if(!remaining.empty())result.tokens.push_back({"sequence",std::move(remaining)});
    result.state=InputState::ground;
  }else result.buffer=data.substr(seqStart);
  return result;
}
std::vector<Token> Tokenizer::feed(const std::string& input){
  auto result=tokenize(value::utf16CodeUnits(input),state_,buffer_,false,x10Mouse_);
  state_=result.state;buffer_=std::move(result.buffer);return std::move(result.tokens);
}
std::vector<Token> Tokenizer::flush(){
  auto result=tokenize({},state_,buffer_,true,x10Mouse_);
  state_=result.state;buffer_=std::move(result.buffer);return std::move(result.tokens);
}
void Tokenizer::reset(){state_=InputState::ground;buffer_.clear();}
std::string Tokenizer::buffer()const{return value::fromUtf16(buffer_);}
}
