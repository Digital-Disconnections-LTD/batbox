#include "cli/remote/IndigenousWebSocket.hpp"
#include "runtime/JsonValue.hpp"
#include "event_loop/AsyncContext.hpp"
#include "runtime/Url.hpp"
#include <libwebsockets.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace batbox::cli::remote {
namespace {
std::string field(const Json& value,const char* name){
  const auto& part=value::property(value,name);
  return part.is_string()?part.get<std::string>():std::string();
}
struct IndigenousConnection {
  event_loop::Loop::Dispatcher owner;
  WebSocketCallbacks callbacks;
  std::string url,address,host,path,proxy,cert,key,ca,passphrase;
  Json headers;
  int port=0;
  bool secure=false;
  std::mutex mutex;
  std::deque<std::pair<std::string,bool>> outgoing;
  bool listeners=true;
  std::atomic_bool stopping=false;
  std::jthread worker;
  std::mutex contextMutex;
  struct lws_context* context=nullptr;
  struct lws* wsi=nullptr;
  std::string incoming;
  int closeCode=1006;
  bool reportedClose=false;
  IndigenousConnection(event_loop::Loop::Dispatcher dispatcher,
      WebSocketCallbacks selected,std::string target,Json extra,
      std::optional<std::string> proxyUrl,std::optional<Json> tls):
    owner(std::move(dispatcher)),callbacks(std::move(selected)),
    url(std::move(target)),headers(std::move(extra)){
    const auto parsed=runtime::parseUrl(url);
    if(!parsed)throw std::invalid_argument("Invalid WebSocket URL");
    const auto scheme=field(*parsed,"protocol");
    if(scheme!="ws:"&&scheme!="wss:")
      throw std::invalid_argument("WebSocket protocol required");
    secure=scheme=="wss:";
    address=field(*parsed,"hostname");
    host=field(*parsed,"host");
    path=field(*parsed,"pathname")+field(*parsed,"search");
    if(path.empty())path="/";
    const auto portText=field(*parsed,"port");
    port=portText.empty()?(secure?443:80):std::stoi(portText);
    if(proxyUrl){
      const auto proxyParts=runtime::parseUrl(*proxyUrl);
      if(!proxyParts)throw std::invalid_argument("Invalid WebSocket proxy URL");
      proxy=field(*proxyParts,"hostname");
      const auto proxyPort=field(*proxyParts,"port");
      if(!field(*proxyParts,"username").empty()){
        proxy=field(*proxyParts,"username")+":"+
          field(*proxyParts,"password")+"@"+proxy;
      }
      proxy+=":"+(proxyPort.empty()?
        (field(*proxyParts,"protocol")=="https:"?"443":"80"):
        proxyPort);
    }
    if(tls){
      cert=field(*tls,"cert");key=field(*tls,"key");
      passphrase=field(*tls,"passphrase");
      const auto& caValue=value::property(*tls,"ca");
      if(caValue.is_string())ca=caValue.get<std::string>();
      else if(caValue.is_array())for(const auto& item:caValue){
        if(!item.is_string())throw std::invalid_argument("CA PEM must be text");
        if(!ca.empty()&&ca.back()!='\n')ca+='\n';
        ca+=item.get<std::string>();
      }
    }
  }
  ~IndigenousConnection(){stop();}
  void wake(){
    std::lock_guard lock(contextMutex);
    if(context)lws_cancel_service(context);
  }
  void emit(std::function<void(const WebSocketCallbacks&)> call){
    WebSocketCallbacks retained;
    {std::lock_guard lock(mutex);if(!listeners)return;retained=callbacks;}
    try{if(owner.valid())owner.postMission(
      [retained=std::move(retained),call=std::move(call)]{
        call(retained);
      });}
    catch(const std::logic_error&){}
  }
  void stop(){
    stopping=true;
    wake();
    if(worker.joinable()&&worker.get_id()!=std::this_thread::get_id())
      worker.join();
  }
  void enqueue(std::string text,bool ping=false){
    if(stopping)throw std::runtime_error("WebSocket closed");
    {std::lock_guard lock(mutex);
      outgoing.emplace_back(std::move(text),ping);}
    wake();
  }
  static int callback(struct lws* wsi,enum lws_callback_reasons reason,
      void*,void* in,size_t len){
    auto* self=static_cast<IndigenousConnection*>(
      lws_context_user(lws_get_context(wsi)));
    if(!self)return 0;
    switch(reason){
      case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:{
        auto** next=static_cast<unsigned char**>(in);
        auto* end=*next+len;
        for(auto it=self->headers.begin();it!=self->headers.end();++it){
          if(!it.value().is_string())continue;
          const auto name=it.key()+":";
          const auto value=it.value().get<std::string>();
          if(lws_add_http_header_by_name(wsi,
              reinterpret_cast<const unsigned char*>(name.c_str()),
              reinterpret_cast<const unsigned char*>(value.data()),
              value.size(),next,end))return -1;
        }
        break;
      }
      case LWS_CALLBACK_CLIENT_ESTABLISHED:{
        self->wsi=wsi;
        char lastId[1024];
        const auto size=lws_hdr_custom_copy(wsi,lastId,sizeof(lastId),
          "x-last-request-id:",18);
        std::optional<std::string> confirmed;
        if(size>=0)confirmed=std::string(lastId,size);
        self->emit([confirmed](const auto& callbacks){
          if(callbacks.onOpen)callbacks.onOpen(confirmed);
        });
        break;
      }
      case LWS_CALLBACK_CLIENT_RECEIVE:{
        self->incoming.append(static_cast<const char*>(in),len);
        if(lws_is_final_fragment(wsi)&&
           lws_remaining_packet_payload(wsi)==0){
          auto data=value::decodeUtf8(self->incoming);
          self->incoming.clear();
          self->emit([data=std::move(data)](const auto& callbacks){
            if(callbacks.onMessage)callbacks.onMessage(data);
          });
        }
        break;
      }
      case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
        self->emit([](const auto& callbacks){
          if(callbacks.onPong)callbacks.onPong();
        });
        break;
      case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        if(len>=2){
          const auto* bytes=static_cast<const unsigned char*>(in);
          self->closeCode=(static_cast<int>(bytes[0])<<8)|bytes[1];
        }
        break;
      case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:{
        const std::string message=in?std::string(static_cast<const char*>(in),len):
          std::string("connection error");
        self->emit([message](const auto& callbacks){
          if(callbacks.onError)callbacks.onError(message);
        });
        if(!self->reportedClose){
          self->reportedClose=true;
          self->emit([](const auto& callbacks){
            if(callbacks.onClose)callbacks.onClose(1006);
          });
        }
        break;
      }
      case LWS_CALLBACK_CLIENT_CLOSED:
        self->wsi=nullptr;
        if(!self->reportedClose){
          self->reportedClose=true;
          const int code=self->closeCode;
          self->emit([code](const auto& callbacks){
            if(callbacks.onClose)callbacks.onClose(code);
          });
        }
        break;
      case LWS_CALLBACK_CLIENT_WRITEABLE:{
        std::pair<std::string,bool> item;
        {std::lock_guard lock(self->mutex);
          if(self->outgoing.empty())break;
          item=std::move(self->outgoing.front());
          self->outgoing.pop_front();}
        std::vector<unsigned char> packet(LWS_PRE+item.first.size());
        std::copy(item.first.begin(),item.first.end(),packet.begin()+LWS_PRE);
        const auto written=lws_write(wsi,packet.data()+LWS_PRE,
          item.first.size(),item.second?LWS_WRITE_PING:LWS_WRITE_TEXT);
        if(written<0||static_cast<std::size_t>(written)<item.first.size()){
          self->emit([](const auto& callbacks){
            if(callbacks.onError)callbacks.onError("WebSocket write failed");
          });
          return -1;
        }
        {std::lock_guard lock(self->mutex);
          if(!self->outgoing.empty())lws_callback_on_writable(wsi);}
        break;
      }
      default:break;
    }
    return 0;
  }
  void run(){
    lws_protocols protocols[2]{};
    protocols[0].name="batbox";
    protocols[0].callback=callback;
    protocols[0].rx_buffer_size=65536;
    lws_context_creation_info info{};
    info.port=CONTEXT_PORT_NO_LISTEN;
    info.protocols=protocols;
    info.user=this;
    info.options=LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    if(!proxy.empty())info.http_proxy_address=proxy.c_str();
    if(!cert.empty()){
      info.client_ssl_cert_mem=cert.data();
      info.client_ssl_cert_mem_len=cert.size();
    }
    if(!key.empty()){
      info.client_ssl_key_mem=key.data();
      info.client_ssl_key_mem_len=key.size();
    }
    if(!ca.empty()){
      info.client_ssl_ca_mem=ca.data();
      info.client_ssl_ca_mem_len=ca.size();
    }
    if(!passphrase.empty())info.client_ssl_private_key_password=
      passphrase.c_str();
    auto* active=lws_create_context(&info);
    {std::lock_guard lock(contextMutex);context=active;}
    if(!active){
      emit([](const auto& callbacks){
        if(callbacks.onError)callbacks.onError("WebSocket context failed");
        if(callbacks.onClose)callbacks.onClose(1006);
      });
      return;
    }
    lws_client_connect_info connect{};
    connect.context=active;
    connect.address=address.c_str();
    connect.port=port;
    connect.path=path.c_str();
    connect.host=host.c_str();
    connect.origin=host.c_str();
    connect.local_protocol_name="batbox";
    connect.ssl_connection=secure?LCCSCF_USE_SSL:0;
    wsi=lws_client_connect_via_info(&connect);
    if(!wsi){
      emit([](const auto& callbacks){
        if(callbacks.onError)callbacks.onError("WebSocket connect failed");
        if(callbacks.onClose)callbacks.onClose(1006);
      });
    }
    while(!stopping){
      if(wsi){
        bool hasQueued;
        {std::lock_guard lock(mutex);hasQueued=!outgoing.empty();}
        if(hasQueued)lws_callback_on_writable(wsi);
      }
      lws_service(active,20);
    }
    {std::lock_guard lock(contextMutex);
      context=nullptr;
      lws_context_destroy(active);
      wsi=nullptr;}
  }
};
}
WebSocketServices makeIndigenousWebSocketServices(event_loop::Loop& loop,
    IndigenousWebSocketConfig config){
  if(!config.getProxyUrl||!config.getTlsOptions||!config.nowMs||
     !config.randomUnit||!config.logDebug||
     !config.registerFlightActivity||!config.unregisterFlightActivity)
    throw std::invalid_argument("Native WebSocket imports required");
  WebSocketServices services;
  services.nowMs=config.nowMs;
  services.randomUnit=config.randomUnit;
  services.logDebug=config.logDebug;
  services.registerFlightActivity=config.registerFlightActivity;
  services.unregisterFlightActivity=config.unregisterFlightActivity;
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  services.remoteMode=config.remoteMode;
  const auto dispatcher=loop.dispatcher();
  services.open=[config=std::move(config),dispatcher](
      const std::string& url,const Json& headers,WebSocketCallbacks callbacks){
    auto state=std::make_shared<IndigenousConnection>(dispatcher,
      std::move(callbacks),url,headers,config.getProxyUrl(url),
      config.getTlsOptions());
    const auto context=runtime::captureAsyncContext();
    state->worker=std::jthread([raw=state.get(),context]{
      runtime::AsyncContextScope scope(context);
      raw->run();
    });
    auto socket=std::make_shared<WebSocketConnection>();
    socket->send=[state](const std::string& text){state->enqueue(text);};
    socket->ping=[state]{state->enqueue({},true);};
    socket->removeListeners=[state]{
      std::lock_guard lock(state->mutex);state->listeners=false;
    };
    socket->close=[state]{state->stop();};
    return socket;
  };
  return services;
}
}
