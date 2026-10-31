#include "transport/Client.hpp"
#include "transport/Errors.hpp"
#include "transport/SSE.hpp"
#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>
using namespace batbox;
using namespace batbox::transport;
namespace {
std::string unhex(const std::string& text) {
  std::string result;
  for (std::size_t i=0;i<text.size();i+=2) result += static_cast<char>(std::stoi(text.substr(i,2),nullptr,16));
  return result;
}
Json error(const APIError& e) {
  return {{"name",e.name()},{"kind",e.kind()},{"message",e.what()},
    {"status",e.status ? Json(*e.status) : Json()},{"body",e.body}};
}
Json run(const Json& test) {
  const auto op=test.at("op").get<std::string>();
  if (op=="prepareMessages") {
    auto prepared=prepareMessageRequest(test.at("params"),test.value("headers",Json::object()));
    return {{"body",prepared.body},{"headers",prepared.headers}};
  }
  if (op=="headers") return getCustomHeaders(test.at("value"));
  if (op=="authHeaders") {
    auto headers=test.value("headers",Json::object());
    bool invoked=false;
    configureApiKeyHeaders(headers,test.value("token",""),test.value("noninteractive",false),[&](bool noninteractive) {
      invoked=true; if (noninteractive!=test.value("noninteractive",false)) throw std::runtime_error("Helper context mismatch");
      return test.value("helperToken","");
    });
    return {{"headers",headers},{"helperInvoked",invoked}};
  }
  if (op=="decode") {
    LineDecoder lines; SSEDecoder decoder;
    Json result={{"lines",Json::array()},{"events",Json::array()},{"parsed",Json::array()}};
    auto append=[&](const std::vector<ServerSentEvent>& events) {
      for (const auto& item:events) {
        result["events"].push_back({{"event",item.event ? Json(*item.event):Json()},{"data",item.data},{"raw",item.raw}});
        try { dispatchSSE(item,Json::object(),[&](const Json& value){result["parsed"].push_back({{"value",value}});}); }
        catch (const APIError& e) { result["parsed"].push_back({{"error",{{"name",e.name()},{"message",e.what()}}}}); }
        catch (const Json::parse_error&) { result["parsed"].push_back({{"error",{{"name","SyntaxError"}}}}); }
      }
    };
    for (const auto& chunk:test.at("chunks")) {
      auto bytes=unhex(chunk.get<std::string>());
      for (const auto& line:lines.decode(bytes)) result["lines"].push_back(line);
      append(decoder.decode(bytes));
    }
    for (const auto& line:lines.flush()) result["lines"].push_back(line);
    append(decoder.flush());
    return result;
  }
  if (op=="request") {
    ClientConfig config;
    config.baseUrl=test.at("url");
    config.requestTimeout=std::chrono::milliseconds(test.value("timeoutMs",5000));
    config.streamIdleTimeout=std::chrono::milliseconds(test.value("idleMs",5000));
    config.headers=test.value("headers",Json::object());
    config.authToken=test.value("token","");
    Client client(config);
    std::stop_source stop;
    std::jthread cancellation;
    if (test.contains("cancelAfterMs")) cancellation=std::jthread([&,wait=test.at("cancelAfterMs").get<int>()] {
      std::this_thread::sleep_for(std::chrono::milliseconds(wait));stop.request_stop();
    });
    Json events=Json::array();
    Json order=Json::array();
    try {
      auto response=client.request(test.at("body"),test.value("stream",false),[&](const Json& part){events.push_back(part);order.push_back("event");},stop.get_token(),
        [&](const Response&){order.push_back("headers");},test.contains("callTimeoutMs") ?
          std::optional(std::chrono::milliseconds(test.at("callTimeoutMs").get<int>())) : std::nullopt);
      return {{"status",response.status},{"headers",response.headers},{"body",response.body},{"events",events},{"order",order}};
    } catch (const APIError& e) { return {{"failure",error(e)},{"events",events}}; }
  }
  throw std::runtime_error("Unknown transport probe operation");
}
} // namespace
int main() {
  for (std::string line;std::getline(std::cin,line);) {
    try { std::cout<<value::stringify(Json({{"result",run(value::parse(line))}}))<<'\n'; }
    catch(const std::exception& e) {std::cout<<value::stringify(Json({{"error",e.what()}}))<<'\n';}
  }
}
