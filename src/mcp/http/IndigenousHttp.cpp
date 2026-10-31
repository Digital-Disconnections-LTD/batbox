#include "mcp/http/Http.hpp"
#include "runtime/Url.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace batbox::mcp::http {
namespace {
std::string lower(std::string name){
  std::transform(name.begin(),name.end(),name.begin(),
    [](unsigned char value){return static_cast<char>(std::tolower(value));});
  return name;
}
struct Transfer {
  const Request& request;
  Response response;
  std::exception_ptr error;
  bool stopped=false;
};
std::size_t receive(char* bytes,std::size_t size,std::size_t count,void* opaque){
  const auto length=size*count;auto& transfer=*static_cast<Transfer*>(opaque);
  try{
    if(transfer.request.onData){
      if(!transfer.request.onData(std::string_view(bytes,length))){
        transfer.stopped=true;return 0;
      }
    }else transfer.response.body.append(bytes,length);
    return length;
  }catch(...){transfer.error=std::current_exception();return 0;}
}
std::size_t receiveHeader(char* bytes,std::size_t size,std::size_t count,void* opaque){
  const auto length=size*count;auto& transfer=*static_cast<Transfer*>(opaque);
  try{
    std::string_view line(bytes,length);
    if(line.starts_with("HTTP/"))transfer.response.headers=Json::object();
    else if(const auto colon=line.find(':');colon!=std::string_view::npos){
      transfer.response.headers[lower(std::string(line.substr(0,colon)))]=
        value::trim(line.substr(colon+1));
    }
    return length;
  }catch(...){transfer.error=std::current_exception();return 0;}
}
int progress(void* opaque,curl_off_t,curl_off_t,curl_off_t,curl_off_t){
  return static_cast<const Transfer*>(opaque)->request.signal.stop_requested()?1:0;
}
struct HeaderList {
  curl_slist* value=nullptr;
  ~HeaderList(){curl_slist_free_all(value);}
  void add(const std::string& line){
    auto* next=curl_slist_append(value,line.c_str());
    if(!next)throw std::bad_alloc();
    value=next;
  }
};
void initialize(){
  static std::once_flag once;
  std::call_once(once,[]{
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
      throw std::runtime_error("MCP HTTP initialization failed");
  });
}
}
std::string Response::header(std::string name)const{
  const auto& value=value::property(headers,lower(std::move(name)));
  return value.is_string()?value.get<std::string>():std::string{};
}
Response fetch(const Request& request){
  if(request.signal.stop_requested())throw std::runtime_error("MCP request cancelled");
  if(!request.headers.is_object()||request.url.empty())
    throw std::invalid_argument("MCP HTTP URL and headers required");
  initialize();
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);
  if(!handle)throw std::runtime_error("MCP HTTP handle unavailable");
  HeaderList headers;
  for(auto it=request.headers.begin();it!=request.headers.end();++it){
    if(!it.value().is_string())throw std::invalid_argument("MCP HTTP header must be a string");
    headers.add(it.key()+": "+it.value().get<std::string>());
  }
  Transfer transfer{request,{},{},false};
  char error[CURL_ERROR_SIZE]{};
  auto* curl=handle.get();
  curl_easy_setopt(curl,CURLOPT_URL,request.url.c_str());
  curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,request.method.c_str());
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers.value);
  curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,request.followRedirects?1L:0L);
  curl_easy_setopt(curl,CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(request.connectTimeout.count()));
  curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,static_cast<long>(request.timeout.count()));
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&transfer);
  curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,receiveHeader);
  curl_easy_setopt(curl,CURLOPT_HEADERDATA,&transfer);
  curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);
  curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&transfer);
  curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,error);
  if(request.method=="POST"||request.method=="PUT"||request.method=="PATCH"||!request.body.empty()){
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,request.body.data());
    curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(request.body.size()));
  }
  const auto result=curl_easy_perform(curl);
  long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
  transfer.response.status=static_cast<int>(status);
  if(transfer.error)std::rethrow_exception(transfer.error);
  if(request.signal.stop_requested())throw std::runtime_error("MCP request cancelled");
  if(result!=CURLE_OK&&!(transfer.stopped&&result==CURLE_WRITE_ERROR))
    throw std::runtime_error(error[0]?error:curl_easy_strerror(result));
  return std::move(transfer.response);
}
std::string form(const Json& values){
  if(!values.is_object())throw std::invalid_argument("OAuth form object required");
  initialize();
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);
  if(!handle)throw std::runtime_error("OAuth form encoder unavailable");
  std::string result;
  const auto encode=[&](const std::string& value){
    std::unique_ptr<char,decltype(&curl_free)> bytes(curl_easy_escape(handle.get(),value.data(),
      static_cast<int>(value.size())),curl_free);
    if(!bytes)throw std::bad_alloc();
    return std::string(bytes.get());
  };
  for(auto it=values.begin();it!=values.end();++it){
    if(it.value().is_null()||it.value().is_discarded())continue;
    if(!result.empty())result+='&';
    result+=encode(it.key())+'='+encode(it.value().is_string()?
      it.value().get<std::string>():value::stringify(it.value()));
  }
  return result;
}
std::string resolveUrl(const std::string& base,const std::string& relative){
  const auto parsed=runtime::parseUrl(relative,base);
  if(!parsed)throw std::runtime_error("Invalid MCP endpoint URL");
  return parsed->at("href").get<std::string>();
}
} // namespace batbox::mcp::http
