#include "diagnostics/LogBrowser.hpp"
#include "diagnostics/LogDates.hpp"
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;

int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const Json input=value::parse(raw);
  Json calls=Json::array(),errors=Json::array();
  event_loop::Loop loop;
  auto lookup=[&input](const std::string& path)->Json{
    for(const auto& file:input.value("files",Json::array()))
      if(std::filesystem::path(input.at("directory").get<std::string>())/
          file.at("name").get<std::string>()==std::filesystem::path(path))return file;
    throw std::runtime_error("File not found");
  };
  diagnostics::LogBrowserServices services;
  services.loop=&loop;
  services.errorsDirectory=[&input]{return input.at("directory").get<std::string>();};
  services.readdir=[&](const std::string& path){
    calls.push_back(Json::array({"readdir",path}));
    event_loop::Deferred<std::vector<std::string>> result(loop);
    if(input.value("readdirReject",false))
      result.reject(std::make_exception_ptr(std::runtime_error("directory failed")));
    else{
      std::vector<std::string> files;
      for(const auto& file:input.value("files",Json::array()))
        files.push_back(file.at("name").get<std::string>());
      result.resolve(std::move(files));
    }
    return result;
  };
  services.readFile=[&](const std::string& path){
    calls.push_back(Json::array({"readFile",path}));
    event_loop::Deferred<std::string> result(loop);
    const auto file=lookup(path);
    if(file.value("readReject",false))result.reject(std::make_exception_ptr(
      std::runtime_error("read failed: "+file.at("name").get<std::string>())));
    else result.resolve(file.at("content").get<std::string>());
    return result;
  };
  services.statMtime=[&](const std::string& path){
    calls.push_back(Json::array({"stat",path}));
    event_loop::Deferred<double> result(loop);
    const auto file=lookup(path);
    if(file.value("statReject",false))result.reject(std::make_exception_ptr(
      std::runtime_error("stat failed: "+file.at("name").get<std::string>())));
    else result.resolve(diagnostics::parseLogISOString(file.at("mtime").get<std::string>()));
    return result;
  };
  services.join=[](const std::string& left,const std::string& right){
    return (std::filesystem::path(left)/right).lexically_normal().string();};
  services.jsonParse=[](const std::string& value){return value::parse(value);};
  services.logError=[&errors](const std::string& message){errors.push_back(message);};
  diagnostics::LogBrowser browser(services);
  Json output=nullptr;
  std::string status="fulfilled";
  const auto operation=input.value("operation",std::string("errors"));
  if(operation=="parse"){
    const auto date=diagnostics::parseLogISOString(input.at("value"));
    output=Json::object({{"time",std::isnan(date)?Json(nullptr):Json(date)},
      {"iso",std::isnan(date)?Json(nullptr):Json(diagnostics::logDateISOString(date))}});
  }else{
    auto result=operation=="index"?
      browser.getErrorLogByIndex(input.at("index").get<double>()):
      operation=="list"?browser.loadLogList(input.at("directory")):browser.loadErrorLogs();
    result.then([&](const Json* value,const std::exception_ptr& error){
      if(error){
        status="rejected";
        try{std::rethrow_exception(error);}
        catch(const std::exception& e){output=e.what();}
      }else output=*value;
    });
    loop.runReady();
  }
  Json out=Json::object({{"status",status},{"result",output},
    {"calls",calls},{"errors",errors}});
  std::cout<<value::stringify(out);
}
