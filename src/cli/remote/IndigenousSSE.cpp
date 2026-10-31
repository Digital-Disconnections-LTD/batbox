#include "cli/remote/IndigenousSSE.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

namespace batbox::cli::remote {
namespace {
struct Headers {
  curl_slist* list=nullptr;
  ~Headers(){curl_slist_free_all(list);}
  Headers(const Headers&)=delete;
  Headers& operator=(const Headers&)=delete;
  Headers():list(nullptr){}
  void add(const Json& value){
    if(!value.is_object())throw std::invalid_argument("HTTP headers required");
    for(auto it=value.begin();it!=value.end();++it){
      const auto line=it.key()+": "+(it.value().is_string()?
        it.value().get<std::string>():value::stringify(it.value()));
      auto* next=curl_slist_append(list,line.c_str());
      if(!next)throw std::bad_alloc();
      list=next;
    }
  }
};
struct CurlHandle {
  CURL* value=curl_easy_init();
  ~CurlHandle(){if(value)curl_easy_cleanup(value);}
  CurlHandle(const CurlHandle&)=delete;
  CurlHandle& operator=(const CurlHandle&)=delete;
  CurlHandle()=default;
};
void initializeCurl(){
  static std::once_flag once;
  std::call_once(once,[]{
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
      throw std::runtime_error("libcurl initialization failed");
  });
}
void postIfOpen(const event_loop::Loop::Dispatcher& owner,
    std::function<void()> callback){
  try{if(owner.valid())owner.postMission(std::move(callback));}
  catch(const std::logic_error&){}
}
struct TlsBlobs {
  std::string ca;
  curl_blob certificate{},privateKey{},caBlob{};
  explicit TlsBlobs(const transport::ClientConfig& config){
    certificate={const_cast<char*>(config.tlsCertificatePem.data()),
      config.tlsCertificatePem.size(),CURL_BLOB_NOCOPY};
    privateKey={const_cast<char*>(config.tlsPrivateKeyPem.data()),
      config.tlsPrivateKeyPem.size(),CURL_BLOB_NOCOPY};
    for(const auto& entry:config.tlsCaPem){
      ca+=entry;if(!entry.empty()&&entry.back()!='\n')ca+='\n';
    }
    caBlob={ca.data(),ca.size(),CURL_BLOB_NOCOPY};
  }
};
void configure(CURL* http,const transport::ClientConfig& config,
    TlsBlobs& blobs){
  curl_easy_setopt(http,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(http,CURLOPT_TCP_KEEPALIVE,
    config.disableKeepAlive?0L:1L);
  curl_easy_setopt(http,CURLOPT_FORBID_REUSE,
    config.disableKeepAlive?1L:0L);
  if(!config.proxyUrl.empty())
    curl_easy_setopt(http,CURLOPT_PROXY,config.proxyUrl.c_str());
  if(!config.unixSocketPath.empty())
    curl_easy_setopt(http,CURLOPT_UNIX_SOCKET_PATH,
      config.unixSocketPath.c_str());
  if(!config.tlsCertificatePem.empty()){
    curl_easy_setopt(http,CURLOPT_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLCERT_BLOB,&blobs.certificate);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERT_BLOB,&blobs.certificate);
  }
  if(!config.tlsPrivateKeyPem.empty()){
    curl_easy_setopt(http,CURLOPT_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLKEY_BLOB,&blobs.privateKey);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEY_BLOB,&blobs.privateKey);
  }
  if(!config.tlsKeyPassphrase.empty()){
    curl_easy_setopt(http,CURLOPT_KEYPASSWD,
      config.tlsKeyPassphrase.c_str());
    curl_easy_setopt(http,CURLOPT_PROXY_KEYPASSWD,
      config.tlsKeyPassphrase.c_str());
  }
  if(!blobs.ca.empty()){
    curl_easy_setopt(http,CURLOPT_CAINFO_BLOB,&blobs.caBlob);
    curl_easy_setopt(http,CURLOPT_PROXY_CAINFO_BLOB,&blobs.caBlob);
  }
  curl_easy_setopt(http,CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(http,CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(http,CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(http,CURLOPT_CONNECTTIMEOUT_MS,
    static_cast<long>(config.connectTimeout.count()));
}
struct StreamJob {
  event_loop::Loop::Dispatcher owner;
  std::atomic_bool abort=false;
  SseStreamCallbacks callbacks;
  int status=0;
  bool headersDelivered=false;
};
std::size_t streamHeader(char* data,std::size_t size,std::size_t count,
    void* opaque){
  const auto length=size*count;
  auto& job=*static_cast<StreamJob*>(opaque);
  std::string_view line(data,length);
  if(line.rfind("HTTP/",0)==0){
    const auto first=line.find(' ');
    if(first!=std::string_view::npos){
      try{job.status=std::stoi(std::string(line.substr(first+1)));}
      catch(...){job.status=0;}
    }
    job.headersDelivered=false;
  }else if(line=="\r\n"||line=="\n"){
    if((job.status>=200&&job.status<300)||job.status>=400){
      const int status=job.status;
      const bool hasBody=status!=204&&status!=205&&status!=304;
      job.headersDelivered=true;
      postIfOpen(job.owner,[callbacks=job.callbacks,status,hasBody]{
        callbacks.onHeaders(status,hasBody);
      });
    }
  }
  return length;
}
std::size_t streamBody(char* data,std::size_t size,std::size_t count,
    void* opaque){
  const auto length=size*count;
  auto& job=*static_cast<StreamJob*>(opaque);
  if(job.abort.load())return 0;
  std::string bytes(data,length);
  postIfOpen(job.owner,[callbacks=job.callbacks,bytes=std::move(bytes)]{
    callbacks.onBytes(bytes);
  });
  return length;
}
int progress(void* opaque,curl_off_t,curl_off_t,curl_off_t,curl_off_t){
  return static_cast<StreamJob*>(opaque)->abort.load()?1:0;
}
void runStream(std::shared_ptr<StreamJob> job,std::string url,Json headers,
    transport::ClientConfig config){
  try{
    initializeCurl();
    CurlHandle curl;
    if(!curl.value)throw std::runtime_error("libcurl handle unavailable");
    Headers prepared;prepared.add(headers);
    TlsBlobs blobs(config);
    configure(curl.value,config,blobs);
    curl_easy_setopt(curl.value,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.value,CURLOPT_HTTPHEADER,prepared.list);
    curl_easy_setopt(curl.value,CURLOPT_HEADERFUNCTION,streamHeader);
    curl_easy_setopt(curl.value,CURLOPT_HEADERDATA,job.get());
    curl_easy_setopt(curl.value,CURLOPT_WRITEFUNCTION,streamBody);
    curl_easy_setopt(curl.value,CURLOPT_WRITEDATA,job.get());
    curl_easy_setopt(curl.value,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl.value,CURLOPT_XFERINFOFUNCTION,progress);
    curl_easy_setopt(curl.value,CURLOPT_XFERINFODATA,job.get());
    const auto result=curl_easy_perform(curl.value);
    if(!job->headersDelivered&&result==CURLE_OK){
      long status=0;curl_easy_getinfo(curl.value,CURLINFO_RESPONSE_CODE,&status);
      postIfOpen(job->owner,[callbacks=job->callbacks,status]{
        callbacks.onHeaders(static_cast<int>(status),true);
      });
    }
    std::optional<std::string> error;
    if(result!=CURLE_OK&&!job->abort.load())error=curl_easy_strerror(result);
    postIfOpen(job->owner,[callbacks=job->callbacks,error]{
      callbacks.onEnd(error);
    });
  }catch(const std::exception& error){
    const std::string message=error.what();
    postIfOpen(job->owner,[callbacks=job->callbacks,message]{
      callbacks.onEnd(message);
    });
  }
}
std::size_t collectBody(char* data,std::size_t size,std::size_t count,
    void* opaque){
  const auto length=size*count;
  static_cast<std::string*>(opaque)->append(data,length);
  return length;
}
void runPost(event_loop::Deferred<SseHttpResponse> result,
    std::string url,Json headers,Json body,transport::ClientConfig config){
  try{
    initializeCurl();
    CurlHandle curl;
    if(!curl.value)throw std::runtime_error("libcurl handle unavailable");
    Headers prepared;prepared.add(headers);
    TlsBlobs blobs(config);
    configure(curl.value,config,blobs);
    const auto serialized=value::stringify(body);
    std::string response;
    curl_easy_setopt(curl.value,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.value,CURLOPT_POST,1L);
    curl_easy_setopt(curl.value,CURLOPT_POSTFIELDS,serialized.data());
    curl_easy_setopt(curl.value,CURLOPT_POSTFIELDSIZE_LARGE,
      static_cast<curl_off_t>(serialized.size()));
    curl_easy_setopt(curl.value,CURLOPT_HTTPHEADER,prepared.list);
    curl_easy_setopt(curl.value,CURLOPT_WRITEFUNCTION,collectBody);
    curl_easy_setopt(curl.value,CURLOPT_WRITEDATA,&response);
    curl_easy_setopt(curl.value,CURLOPT_TIMEOUT_MS,
      static_cast<long>(config.requestTimeout.count()));
    const auto code=curl_easy_perform(curl.value);
    if(code!=CURLE_OK)
      throw std::runtime_error(curl_easy_strerror(code));
    long status=0;curl_easy_getinfo(curl.value,CURLINFO_RESPONSE_CODE,&status);
    auto parsed=value::parse(response,false);
    if(parsed.is_discarded())parsed=response;
    if(result.dispatcher().valid())
      result.resolve(SseHttpResponse{static_cast<int>(status),std::move(parsed)});
  }catch(...){
    try{if(result.dispatcher().valid())result.reject(std::current_exception());}
    catch(const std::logic_error&){}
  }
}
}
SseServices makeIndigenousSseServices(event_loop::Loop& loop,
    IndigenousSseConfig config){
  if(!config.getAuthHeaders||!config.userBat||!config.logDebug||
     config.protocolVersionHeaderName.empty())
    throw std::invalid_argument("Native SSE configuration imports required");
  SseServices services;
  const auto owner=loop.dispatcher();
  const auto network=config.network;
  services.openStream=[owner,network](const std::string& url,
      const Json& headers,SseStreamCallbacks callbacks){
    auto job=std::make_shared<StreamJob>();
    job->owner=owner;job->callbacks=std::move(callbacks);
    auto handle=std::make_shared<SseStreamHandle>();
    handle->abort=[job]{job->abort.store(true);};
    runtime::contextThread([job,url,headers,network]{
      runStream(job,url,headers,network);
    }).detach();
    return handle;
  };
  services.post=[owner,network](const std::string& url,
      const Json& headers,const Json& body){
    event_loop::Deferred<SseHttpResponse> result(owner);
    runtime::contextThread([result,url,headers,body,network]{
      runPost(result,url,headers,body,network);
    }).detach();
    return result;
  };
  services.getAuthHeaders=std::move(config.getAuthHeaders);
  services.userBat=std::move(config.userBat);
  services.logDebug=std::move(config.logDebug);
  services.protocolVersionHeaderName=
    std::move(config.protocolVersionHeaderName);
  services.protocolVersionHeaderValue=
    std::move(config.protocolVersionHeaderValue);
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  services.nowMs=[]{
    return static_cast<double>(std::chrono::duration_cast<
      std::chrono::milliseconds>(std::chrono::system_clock::now()
        .time_since_epoch()).count());
  };
  services.randomUnit=[]{
    thread_local std::mt19937_64 engine(std::random_device{}());
    return std::generate_canonical<double,53>(engine);
  };
  return services;
}
}
