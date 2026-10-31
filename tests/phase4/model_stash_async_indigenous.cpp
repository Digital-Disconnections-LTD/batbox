#include "model_stash/AsyncDetection.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(){
  using batbox::Json;
  batbox::event_loop::Loop loop;
  std::vector<std::string> trace;
  const auto directory=std::filesystem::temp_directory_path()/"batbox-cache-async-native";
  batbox::model_stash::Services services;
  services.excludedModelFamily="fast";
  services.nowMs=[](){return 0.0;};
  services.parseTimestampMs=[](const std::string&){return 0.0;};
  services.logForDebugging=[&](const std::string& text,const std::string& level){
    trace.push_back(level+":"+text);};
  services.logError=[&](const std::string& text){trace.push_back("error:"+text);};
  auto detector=std::make_shared<batbox::model_stash::Detector>(services);
  auto writer=batbox::model_stash::DiffWriterServices{
    [directory](){return directory.string();},[](){return 0.0;}};
  auto check=batbox::model_stash::makeAsyncStashCheck(loop,detector,writer);
  auto snapshot=[](const std::string& text){return Json{{"system",Json::array({
      Json{{"text",text}}})},{"toolSchemas",Json::array()},
      {"querySource","repl_main_thread"},{"model","standard"}};};
  detector->recordPoetryState(snapshot("before"));
  check("repl_main_thread",10000,0,Json::array(),nullptr,std::nullopt);
  detector->recordPoetryState(snapshot("after"));
  trace.push_back("before call");
  check("repl_main_thread",1000,9000,Json::array(),nullptr,std::nullopt);
  trace.push_back("after call");
  assert(trace.size()==2);
  assert(detector->stateFor("repl_main_thread")->pendingChanges.has_value());
  loop.run();
  assert(trace.size()==3);
  assert(trace[2].starts_with("warn:[PROMPT CACHE BREAK] system prompt changed"));
  assert(!detector->stateFor("repl_main_thread")->pendingChanges.has_value());
  const auto file=directory/"cache-break-aaaa.diff";
  std::ifstream stream(file,std::ios::binary);
  assert(stream.good());
  std::string patch((std::istreambuf_iterator<char>(stream)),{});
  assert(patch.starts_with("Index: prompt-state\n"));
  assert(patch.find("-before\n+after\n")!=std::string::npos);
  std::filesystem::remove_all(directory);
  std::cout<<"cache async owner and local diff file pass\n";
}
