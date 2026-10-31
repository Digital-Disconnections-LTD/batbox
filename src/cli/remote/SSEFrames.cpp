#include "cli/remote/SSEFrames.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::cli::remote {
SSEParseResult parseSSEFrames(std::string_view buffer){
  SSEParseResult result;
  std::size_t pos=0;
  while(true){
    const auto boundary=buffer.find("\n\n",pos);
    if(boundary==std::string_view::npos)break;
    const auto raw=buffer.substr(pos,boundary-pos);
    pos=boundary+2;
    if(value::trim(raw).empty())continue;
    SSEFrame frame;bool comment=false;
    std::size_t lineStart=0;
    while(lineStart<=raw.size()){
      const auto lineEnd=raw.find('\n',lineStart);
      const auto line=raw.substr(lineStart,lineEnd==std::string_view::npos?
        std::string_view::npos:lineEnd-lineStart);
      if(!line.empty()&&line.front()==':')comment=true;
      else if(const auto colon=line.find(':');colon!=std::string_view::npos){
        const auto field=line.substr(0,colon);
        auto value=line.substr(colon+1);
        if(!value.empty()&&value.front()==' ')value.remove_prefix(1);
        if(field=="event")frame.event=std::string(value);
        else if(field=="id")frame.id=std::string(value);
        else if(field=="data"){
          if(frame.data&&!frame.data->empty())
            frame.data=*frame.data+"\n"+std::string(value);
          else frame.data=std::string(value);
        }
      }
      if(lineEnd==std::string_view::npos)break;
      lineStart=lineEnd+1;
    }
    if((frame.data&&!frame.data->empty())||comment)
      result.frames.push_back(std::move(frame));
  }
  result.remaining=std::string(buffer.substr(pos));
  return result;
}
}
