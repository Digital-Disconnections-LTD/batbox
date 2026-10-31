#include "diagnostics/DiagLogs.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using batbox::Json;
  const auto input=Json::parse(std::cin);
  batbox::event_loop::Loop loop;
  double now=0;
  Json lines=Json::array();
  batbox::diagnostics::Services services;
  services.diagnosticFile=[](){return std::optional<std::string>("/tmp/diag.jsonl");};
  services.timestamp=[](){return std::string("2026-10-28T12:00:00.000Z");};
  services.nowMs=[&](){return now;};
  services.appendFileSync=[&](const std::string&,const std::string& line){
    lines.push_back(line);};
  services.mkdirSync=[](const std::string&){};
  auto logger=std::make_shared<batbox::diagnostics::Logger>(services);
  batbox::event_loop::Deferred<Json> operation(loop);
  const auto kind=input["kind"].get<std::string>();
  auto wrapped=batbox::diagnostics::withDiagnosticsTiming<Json>(loop,logger,
    "work",std::function<batbox::event_loop::Deferred<Json>()>([&]{
      if(kind=="throw")throw std::runtime_error("operation failed");
      return operation;
    }),kind=="getDataThrow"?std::function<Json(const Json&)>([](const Json&)->Json{
      throw std::runtime_error("data failed");
    }):input.value("includeData",false)?std::function<Json(const Json&)>(
      [](const Json& value){return Json{{"value",value},{"duration_ms",77}};}):
      std::function<Json(const Json&)>{});
  const auto before=lines;
  now=input["endMs"].get<double>();
  if(kind=="resolve"||kind=="getDataThrow")operation.resolve(input["value"]);
  if(kind=="reject")operation.reject(std::make_exception_ptr(
    std::runtime_error("operation rejected")));
  Json outcome;
  wrapped.then([&](const Json* value,const std::exception_ptr& error){
    if(error){
      try{std::rethrow_exception(error);}catch(const std::exception& e){
        outcome={{"error",e.what()}};}
    }else outcome={{"value",*value}};
    loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  std::cout<<Json{{"before",before},{"after",lines},{"outcome",outcome}}.dump()<<'\n';
}
