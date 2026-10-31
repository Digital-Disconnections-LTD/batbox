#include "gadgets/bash/AsyncBashImage.hpp"
#include "gadgets/bash/Helpers.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bash_gadget {
namespace {
std::string decodeBase64(const std::string& input){
  std::string bytes;bytes.reserve(input.size()*3/4);
  unsigned value=0,bits=0;
  for(unsigned char ch:input){
    if(ch=='=')break;
    unsigned digit;
    if(ch>='A'&&ch<='Z')digit=ch-'A';
    else if(ch>='a'&&ch<='z')digit=ch-'a'+26;
    else if(ch>='0'&&ch<='9')digit=ch-'0'+52;
    else if(ch=='+'||ch=='-')digit=62;
    else if(ch=='/'||ch=='_')digit=63;
    else continue;
    value=(value<<6)|digit;bits+=6;
    if(bits>=8){bits-=8;bytes.push_back(static_cast<char>((value>>bits)&255));}
  }
  return bytes;
}
std::string encodeBase64(const std::string& bytes){
  static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;result.reserve(((bytes.size()+2)/3)*4);
  for(std::size_t i=0;i<bytes.size();i+=3){
    const unsigned a=static_cast<unsigned char>(bytes[i]);
    const unsigned b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
    const unsigned c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
    result+=alphabet[a>>2];result+=alphabet[((a&3)<<4)|(b>>4)];
    result+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
    result+=i+2<bytes.size()?alphabet[c&63]:'=';
  }
  return result;
}
struct State:std::enable_shared_from_this<State>{
  event_loop::Loop& loop;
  std::string source;
  std::optional<std::string> path;
  std::optional<std::uintmax_t> size;
  AsyncBashImageOps operations;
  event_loop::Deferred<std::optional<std::string>> output;
  State(event_loop::Loop& owner,std::string text,std::optional<std::string> file,
    std::optional<std::uintmax_t> length,AsyncBashImageOps ops):
    loop(owner),source(std::move(text)),path(std::move(file)),size(length),
    operations(std::move(ops)),output(owner){}
  void start(){
    try{
      if(path&&!path->empty()){
        if(!size){
          if(!operations.stat)throw std::invalid_argument("Bash image stat import required");
          operations.stat(*path).then([self=shared_from_this()](
            const std::uintmax_t* value,const std::exception_ptr& error){
            if(error){self->output.reject(error);return;}
            self->size=*value;self->afterStat();
          });
        }else afterStat();
      }else afterRead();
    }catch(...){output.reject(std::current_exception());}
  }
  void afterStat(){
    try{
      if(*size>20ull*1024*1024){output.resolve(std::nullopt);return;}
      if(!operations.readFile)throw std::invalid_argument("Bash image readFile import required");
      operations.readFile(*path).then([self=shared_from_this()](
        const std::string* value,const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        self->source=*value;self->afterRead();
      });
    }catch(...){output.reject(std::current_exception());}
  }
  void afterRead(){
    try{
      const auto parsed=parseDataUri(source);
      if(!parsed){output.resolve(std::nullopt);return;}
      const std::string mediaType=parsed->at("mediaType").get<std::string>();
      const std::string data=parsed->at("data").get<std::string>();
      const auto slash=mediaType.find('/');
      const auto second=slash==std::string::npos?std::string::npos:mediaType.find('/',slash+1);
      std::string ext=slash==std::string::npos?"":mediaType.substr(slash+1,
        second==std::string::npos?std::string::npos:second-slash-1);
      if(ext.empty())ext="png";
      if(!operations.resizeBuffer)throw std::invalid_argument("Bash image resize import required");
      operations.resizeBuffer(decodeBase64(data),ext).then([self=shared_from_this()](
        const BashImageTransform* resized,const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        self->output.resolve("data:image/"+resized->mediaType+";base64,"+
          encodeBase64(resized->buffer));
      });
    }catch(...){output.reject(std::current_exception());}
  }
};
} // namespace
event_loop::Deferred<std::optional<std::string>> resizeShellImageOutputAsync(
  event_loop::Loop& loop,std::string stdoutText,
  std::optional<std::string> outputFilePath,
  std::optional<std::uintmax_t> outputFileSize,const AsyncBashImageOps& operations){
  if(!loop.isOwnerThread())throw std::logic_error("Bash image resize owner required");
  auto state=std::make_shared<State>(loop,std::move(stdoutText),
    std::move(outputFilePath),outputFileSize,operations);
  auto output=state->output;state->start();return output;
}
} // namespace batbox::bash_gadget
