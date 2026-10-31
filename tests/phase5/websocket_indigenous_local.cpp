#include "cli/remote/IndigenousWebSocket.hpp"
#include "runtime/Url.hpp"
#include <libwebsockets.h>
#include <atomic>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace batbox;
namespace {
struct Server {
  std::atomic_bool stop=false;
  std::string received;
  std::string outgoing;
  lws_context* context=nullptr;
  static int callback(lws* wsi,lws_callback_reasons reason,
      void*,void* in,size_t length){
    auto* self=static_cast<Server*>(lws_context_user(lws_get_context(wsi)));
    if(!self)return 0;
    if(reason==LWS_CALLBACK_RECEIVE){
      self->received.append(static_cast<const char*>(in),length);
      self->outgoing=self->received;
      lws_callback_on_writable(wsi);
    }else if(reason==LWS_CALLBACK_SERVER_WRITEABLE&&
             !self->outgoing.empty()){
      std::vector<unsigned char> data(LWS_PRE+self->outgoing.size());
      std::copy(self->outgoing.begin(),self->outgoing.end(),
        data.begin()+LWS_PRE);
      const auto sent=lws_write(wsi,data.data()+LWS_PRE,
        self->outgoing.size(),LWS_WRITE_TEXT);
      if(sent<0)return -1;
      self->outgoing.clear();
    }
    return 0;
  }
};
}
int main(){
  lws_set_log_level(LLL_ERR,nullptr);
  Server server;
  lws_protocols protocols[2]{};
  protocols[0].name="batbox";
  protocols[0].callback=Server::callback;
  protocols[0].rx_buffer_size=65536;
  lws_context_creation_info info{};
  info.port=0;info.vhost_name="batbox-test";info.protocols=protocols;
  info.user=&server;
  server.context=lws_create_context(&info);
  assert(server.context);
  auto* vhost=lws_get_vhost_by_name(server.context,"batbox-test");
  assert(vhost);
  const int port=lws_get_vhost_listen_port(vhost);
  assert(port>0);
  std::jthread worker([&]{
    while(!server.stop)lws_service(server.context,10);
  });
  event_loop::Loop loop;
  cli::remote::IndigenousWebSocketConfig config;
  config.getProxyUrl=[](const std::string&)
      ->std::optional<std::string>{return {};};
  config.getTlsOptions=[]()->std::optional<Json>{return {};};
  config.nowMs=[](){return static_cast<double>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());};
  config.randomUnit=[]{return 0.5;};
  config.logDebug=[](const std::string&,const Json&){};
  config.registerFlightActivity=[](auto){};
  config.unregisterFlightActivity=[]{};
  auto services=cli::remote::makeIndigenousWebSocketServices(loop,config);
  const auto url=runtime::parseUrl("ws://127.0.0.1:"+
    std::to_string(port)+"/session");
  assert(url);
  cli::remote::WebSocketOptions options;
  options.autoReconnect=false;
  auto transport=std::make_shared<cli::remote::WebSocketTransport>(loop,
    *url,Json::object(),"session-1",[](){return Json::object();},
    services,options);
  bool connected=false;
  std::string received;
  transport->setOnConnect([&]{
    connected=true;
    transport->write(Json{{"type","keep_alive"}});
  });
  transport->setOnData([&](const std::string& data){received=data;});
  transport->connect();
  const auto deadline=std::chrono::steady_clock::now()+
    std::chrono::seconds(5);
  while(received.empty()&&std::chrono::steady_clock::now()<deadline){
    loop.runReady();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  assert(connected);
  assert(received=="{\"type\":\"keep_alive\"}\n");
  transport->close();
  server.stop=true;
  lws_cancel_service(server.context);
  worker.join();
  lws_context_destroy(server.context);
}
