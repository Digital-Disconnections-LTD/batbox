#include "gadget_results/Storage.hpp"
#include "formatting/Format.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace batbox::gadget_results {
namespace {
// Walk UTF-8 scalars without materialising a per-character vector; a large
// tool result is scanned here several times before it reaches the model.
template<class Visit>void forEachScalar(std::string_view text,Visit&& visit){
  for(std::size_t i=0;i<text.size();){
    auto c=static_cast<unsigned char>(text[i++]);
    std::uint32_t cp=c;int remaining=0;
    if((c&0xe0)==0xc0){cp=c&0x1f;remaining=1;}
    else if((c&0xf0)==0xe0){cp=c&0x0f;remaining=2;}
    else if((c&0xf8)==0xf0){cp=c&0x07;remaining=3;}
    else if(c>=0x80)throw std::invalid_argument("Invalid UTF-8");
    const auto width=remaining;
    while(remaining--){
      if(i==text.size())throw std::invalid_argument("Incomplete UTF-8");
      c=static_cast<unsigned char>(text[i++]);
      if((c&0xc0)!=0x80)throw std::invalid_argument("Invalid UTF-8 continuation");
      cp=(cp<<6)|(c&0x3f);
    }
    if((width==1&&cp<0x80)||(width==2&&cp<0x800)||(width==3&&cp<0x10000)||cp>0x10ffff)
      throw std::invalid_argument("Invalid UTF-8 scalar");
    visit(cp);
  }
}
bool space(std::uint32_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||(c>=0x2000&&c<=0x200a)||
    c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
bool blank(std::string_view text){
  bool result=true;
  forEachScalar(text,[&](std::uint32_t cp){if(!space(cp))result=false;});
  return result;
}
std::size_t utf16Length(std::string_view text){
  std::size_t length=0;
  forEachScalar(text,[&](std::uint32_t cp){length+=cp>0xffff?2:1;});
  return length;
}
}
double getPersistenceThreshold(const std::string& name,double declared,const Json& overrides){
  if(!std::isfinite(declared))return declared;
  const auto& value=value::property(overrides,name);
  if(value.is_number()){
    const double override=value.get<double>();
    if(std::isfinite(override)&&override>0)return override;
  }
  return std::min(declared,DEFAULT_MAX_RESULT_SIZE_CHARS);
}
double getPerMessageBudgetLimit(const Json& value){
  if(value.is_number()){
    const double override=value.get<double>();
    if(std::isfinite(override)&&override>0)return override;
  }
  return MAX_GADGET_RESULTS_PER_MESSAGE_CHARS;
}
Json generatePreview(const std::string& content,double maxBytes){
  if(value::stringLength(content)<=maxBytes)return {{"preview",content},{"hasMore",false}};
  const auto truncated=value::slice(content,0,maxBytes);
  const auto units=value::utf16CodeUnits(truncated);const auto last=units.rfind(u'\n');
  const double lastNewline=last==std::u16string::npos?-1:static_cast<double>(last);
  const double cut=lastNewline>maxBytes*.5?lastNewline:maxBytes;
  return {{"preview",value::slice(content,0,cut)},{"hasMore",true}};
}
std::string buildLargeGadgetResultMessage(const Json& result){
  std::string message=std::string(PERSISTED_OUTPUT_TAG)+"\n";
  message+="Output too large ("+formatting::formatFileSize(result.at("originalSize"))+ "). Full output saved to: "+result.at("filepath").get<std::string>()+"\n\n";
  message+="Preview (first "+formatting::formatFileSize(PREVIEW_SIZE_BYTES)+"):\n";
  message+=result.at("preview").get<std::string>();
  message+=result.at("hasMore").get<bool>()?"\n...\n":"\n";
  return message+PERSISTED_OUTPUT_CLOSING_TAG;
}
bool isPersistError(const Json& result){return result.is_object()&&result.contains("error");}
std::string getFileSystemErrorMessage(const FileSystemError& error){
  if(!error.code||error.code->empty())return error.what();
  const auto path=error.path.value_or("unknown path");
  if(*error.code=="ENOENT")return "Directory not found: "+path;
  if(*error.code=="EACCES")return "Permission denied: "+path;
  if(*error.code=="ENOSPC")return "No space left on device";
  if(*error.code=="EROFS")return "Read-only file system";
  if(*error.code=="EMFILE")return "Too many open files";
  if(*error.code=="EEXIST")return "File already exists: "+path;
  return *error.code+": "+error.what();
}
bool isGadgetResultContentEmpty(const Json& content){
  if(!value::truthy(content))return true;
  if(content.is_string())return blank(content.get_ref<const std::string&>());
  if(!content.is_array())return false;
  for(const auto& block:content){
    if(block.is_null())throw std::runtime_error("Cannot use 'in' operator to search for 'type' in null");
    if(!block.is_object()||!block.contains("type")||block.at("type")!="text"||!block.contains("text"))return false;
    const auto& text=block.at("text");
    if(text.is_string()&&!blank(text.get_ref<const std::string&>()))return false;
  }
  return true;
}
bool hasImageBlock(const Json& content){
  if(!content.is_array())return false;
  for(const auto& block:content){
    if(block.is_null())throw std::runtime_error("Cannot use 'in' operator to search for 'type' in null");
    if(block.is_object()&&value::property(block,"type")=="image")return true;
  }
  return false;
}
bool isContentAlreadyCompacted(const Json& content){
  return content.is_string()&&content.get_ref<const std::string&>().starts_with(PERSISTED_OUTPUT_TAG);
}
double contentSize(const Json& content){
  if(content.is_string())return utf16Length(content.get_ref<const std::string&>());
  double size=0;
  for(const auto& block:content)if(value::property(block,"type")=="text")
    size+=utf16Length(block.at("text").get_ref<const std::string&>());
  return size;
}
} // namespace batbox::gadget_results
