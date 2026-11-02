#include "application/ErrorReport.hpp"
#include "runtime/JsonValue.hpp"
#include <new>
#include <stdexcept>
#include <typeinfo>
#if defined(__GNUG__)
#include <cxxabi.h>
#include <cstdlib>
#include <memory>
#endif

namespace batbox::application {
namespace {
std::string typeName(const std::type_info& type){
#if defined(__GNUG__)
  int status=0;
  std::unique_ptr<char,void(*)(void*)> name(
    abi::__cxa_demangle(type.name(),nullptr,nullptr,&status),std::free);
  if(status==0&&name)return name.get();
#endif
  return type.name();
}
}

std::string statusLine(std::string_view text){
  std::string out;
  out.reserve(text.size());
  for(std::size_t i=0;i<text.size();++i){
    const auto byte=static_cast<unsigned char>(text[i]);
    // Terminal escape sequences (colors, titles, links) are dropped whole.
    if(byte==0x1b&&i+1<text.size()&&(text[i+1]=='['||text[i+1]==']')){
      const bool csi=text[i+1]=='[';
      std::size_t end=i+2;
      while(end<text.size()){
        const auto code=static_cast<unsigned char>(text[end]);
        if(csi&&code>=0x40&&code<=0x7e)break;
        if(!csi&&(code==0x07||(code==0x1b&&end+1<text.size()&&text[end+1]=='\\'))){
          if(code==0x1b)++end;
          break;
        }
        ++end;
      }
      i=end;
      if(out.empty()||out.back()!=' ')out.push_back(' ');
      continue;
    }
    const char next=byte<0x20||byte==0x7f?' ':text[i];
    if(next==' '&&(out.empty()||out.back()==' '))continue;
    out.push_back(next);
  }
  while(!out.empty()&&out.front()==' ')out.erase(0,1);
  while(!out.empty()&&out.back()==' ')out.pop_back();
  return out;
}

ReportedError describeError(std::exception_ptr error){
  ReportedError out;
  if(!error)return out;
  try{std::rethrow_exception(error);}
  catch(const Json::exception& failure){
    out={failure.what(),typeName(typeid(failure)),true};
  }catch(const std::logic_error& failure){
    out={failure.what(),typeName(typeid(failure)),true};
  }catch(const std::bad_alloc& failure){
    out={failure.what(),typeName(typeid(failure)),true};
  }catch(const std::exception& failure){
    out={failure.what(),typeName(typeid(failure)),false};
  }catch(...){
    out={"unknown exception","non-standard exception",true};
  }
  // A message with no space is a library token such as "basic_string",
  // including one a command UI already flattened into a runtime_error.
  const auto line=statusLine(out.what);
  if(line.empty()||line.find(' ')==std::string::npos)out.internal=true;
  return out;
}

std::string statusErrorText(const ReportedError& error,std::string_view origin){
  if(error.what.empty()&&error.type.empty())return {};
  if(error.internal)
    return "Error: internal error in "+std::string(origin)+" · details in native.log";
  return "Error: "+statusLine(error.what);
}

std::string errorLogLine(const ReportedError& error,std::string_view origin){
  return "Error in "+std::string(origin)+": "+
    (error.type.empty()?std::string{}:error.type+": ")+error.what;
}

} // namespace batbox::application
