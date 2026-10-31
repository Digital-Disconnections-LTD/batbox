#include "cli/remote/IndigenousCcrHttp.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/JsonValue.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
struct CurlHandle {
  CURL* value=curl_easy_init();
  CurlHandle()=default;
  ~CurlHandle(){if(value)curl_easy_cleanup(value);}
};
struct Headers {
  curl_slist* value=nullptr;
  ~Headers(){curl_slist_free_all(value);}
  void add(const Json& headers){
    if(!headers.is_object())throw std::invalid_argument("CCR headers required");
    for(auto it=headers.begin();it!=headers.end();++it){
      const auto line=it.key()+": "+(it.value().is_string()?
        it.value().get<std::string>():value::stringify(it.value()));
      auto* next=curl_slist_append(value,line.c_str());
      if(!next)throw std::bad_alloc();
      value=next;
    }
  }
};
struct ResponseBuffer {std::string body;Json headers=Json::object();};
std::size_t bodyCallback(char* bytes,std::size_t size,std::size_t count,
    void* opaque){
  const auto length=size*count;
  static_cast<ResponseBuffer*>(opaque)->body.append(bytes,length);
  return length;
}
std::size_t headerCallback(char* bytes,std::size_t size,std::size_t count,
    void* opaque){
  const auto length=size*count;
  auto& response=*static_cast<ResponseBuffer*>(opaque);
  std::string_view line(bytes,length);
  if(line.rfind("HTTP/",0)==0)response.headers=Json::object();
  else{
    const auto at=line.find(':');
    if(at!=std::string_view::npos){
      std::string name(line.substr(0,at));
      std::transform(name.begin(),name.end(),name.begin(),
        [](unsigned char ch){return std::tolower(ch);});
      std::string value(line.substr(at+1));
      const auto first=value.find_first_not_of(" \t\r\n");
      const auto last=value.find_last_not_of(" \t\r\n");
      value=first==std::string::npos?std::string():
        value.substr(first,last-first+1);
      response.headers[name]=value;
    }
  }
  return length;
}
void initializeCurl(){
  static std::once_flag once;
  std::call_once(once,[]{
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
      throw std::runtime_error("libcurl initialization failed");
  });
}
void execute(Deferred<CcrHttpResponse> result,std::string method,
    std::string url,Json body,Json headers,double timeoutMs,
    transport::ClientConfig config){
  try{
    initializeCurl();
    CurlHandle curl;
    if(!curl.value)throw std::runtime_error("libcurl handle unavailable");
    Headers prepared;prepared.add(headers);
    ResponseBuffer response;
    std::string serialized;
    if(method!="get")serialized=value::stringify(body);
    curl_easy_setopt(curl.value,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.value,CURLOPT_HTTPHEADER,prepared.value);
    curl_easy_setopt(curl.value,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl.value,CURLOPT_TCP_KEEPALIVE,
      config.disableKeepAlive?0L:1L);
    curl_easy_setopt(curl.value,CURLOPT_FORBID_REUSE,
      config.disableKeepAlive?1L:0L);
    if(method=="post")curl_easy_setopt(curl.value,CURLOPT_POST,1L);
    else if(method=="put")curl_easy_setopt(curl.value,CURLOPT_CUSTOMREQUEST,"PUT");
    if(method!="get"){
      curl_easy_setopt(curl.value,CURLOPT_POSTFIELDS,serialized.data());
      curl_easy_setopt(curl.value,CURLOPT_POSTFIELDSIZE_LARGE,
        static_cast<curl_off_t>(serialized.size()));
    }
    if(!config.proxyUrl.empty())
      curl_easy_setopt(curl.value,CURLOPT_PROXY,config.proxyUrl.c_str());
    if(!config.unixSocketPath.empty())
      curl_easy_setopt(curl.value,CURLOPT_UNIX_SOCKET_PATH,
        config.unixSocketPath.c_str());
    curl_blob cert{const_cast<char*>(config.tlsCertificatePem.data()),
      config.tlsCertificatePem.size(),CURL_BLOB_NOCOPY};
    curl_blob key{const_cast<char*>(config.tlsPrivateKeyPem.data()),
      config.tlsPrivateKeyPem.size(),CURL_BLOB_NOCOPY};
    if(!config.tlsCertificatePem.empty()){
      curl_easy_setopt(curl.value,CURLOPT_SSLCERTTYPE,"PEM");
      curl_easy_setopt(curl.value,CURLOPT_SSLCERT_BLOB,&cert);
      curl_easy_setopt(curl.value,CURLOPT_PROXY_SSLCERTTYPE,"PEM");
      curl_easy_setopt(curl.value,CURLOPT_PROXY_SSLCERT_BLOB,&cert);
    }
    if(!config.tlsPrivateKeyPem.empty()){
      curl_easy_setopt(curl.value,CURLOPT_SSLKEYTYPE,"PEM");
      curl_easy_setopt(curl.value,CURLOPT_SSLKEY_BLOB,&key);
      curl_easy_setopt(curl.value,CURLOPT_PROXY_SSLKEYTYPE,"PEM");
      curl_easy_setopt(curl.value,CURLOPT_PROXY_SSLKEY_BLOB,&key);
    }
    if(!config.tlsKeyPassphrase.empty()){
      curl_easy_setopt(curl.value,CURLOPT_KEYPASSWD,
        config.tlsKeyPassphrase.c_str());
      curl_easy_setopt(curl.value,CURLOPT_PROXY_KEYPASSWD,
        config.tlsKeyPassphrase.c_str());
    }
    std::string ca;
    for(const auto& pem:config.tlsCaPem){
      ca+=pem;if(!pem.empty()&&pem.back()!='\n')ca+='\n';
    }
    curl_blob caBlob{ca.data(),ca.size(),CURL_BLOB_NOCOPY};
    if(!ca.empty()){
      curl_easy_setopt(curl.value,CURLOPT_CAINFO_BLOB,&caBlob);
      curl_easy_setopt(curl.value,CURLOPT_PROXY_CAINFO_BLOB,&caBlob);
    }
    curl_easy_setopt(curl.value,CURLOPT_ACCEPT_ENCODING,"");
    curl_easy_setopt(curl.value,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl.value,CURLOPT_MAXREDIRS,20L);
    curl_easy_setopt(curl.value,CURLOPT_CONNECTTIMEOUT_MS,
      static_cast<long>(config.connectTimeout.count()));
    curl_easy_setopt(curl.value,CURLOPT_TIMEOUT_MS,
      static_cast<long>(timeoutMs));
    curl_easy_setopt(curl.value,CURLOPT_WRITEFUNCTION,bodyCallback);
    curl_easy_setopt(curl.value,CURLOPT_WRITEDATA,&response);
    curl_easy_setopt(curl.value,CURLOPT_HEADERFUNCTION,headerCallback);
    curl_easy_setopt(curl.value,CURLOPT_HEADERDATA,&response);
    const auto code=curl_easy_perform(curl.value);
    if(code!=CURLE_OK)
      throw std::runtime_error(curl_easy_strerror(code));
    long status=0;curl_easy_getinfo(curl.value,CURLINFO_RESPONSE_CODE,&status);
    Json data=value::parse(response.body,false);
    if(data.is_discarded())data=response.body;
    if(result.dispatcher().valid())
      result.resolve(CcrHttpResponse{static_cast<int>(status),
        std::move(response.headers),std::move(data)});
  }catch(...){
    try{if(result.dispatcher().valid())result.reject(std::current_exception());}
    catch(const std::logic_error&){}
  }
}
}
std::function<Deferred<CcrHttpResponse>(const std::string&,
    const std::string&,const Json&,const Json&,double)>
makeIndigenousCcrHttp(event_loop::Loop& loop,transport::ClientConfig config){
  const auto owner=loop.dispatcher();
  return [owner,config=std::move(config)](const std::string& method,
      const std::string& url,const Json& body,const Json& headers,
      double timeoutMs){
    Deferred<CcrHttpResponse> result(owner);
    runtime::contextThread([result,method,url,body,headers,timeoutMs,config]{
      execute(result,method,url,body,headers,timeoutMs,config);
    }).detach();
    return result;
  };
}
}
