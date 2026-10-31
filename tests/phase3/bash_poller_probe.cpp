#include "shell/MissionOutputPoller.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(data);batbox::Json output=batbox::Json::array();
  for(const auto& ticks:cases){
    batbox::Json calls=batbox::Json::array();std::size_t previous=0;
    for(const auto& tick:ticks){
      batbox::shell::TailResult tail{tick.at("content").get<std::string>(),
        tick.at("bytesRead").get<std::size_t>(),tick.at("bytesTotal").get<std::size_t>()};
      const auto sample=batbox::shell::sampleMissionOutputTail(tail,previous);
      if(!tail.content.empty())previous=sample.totalLines;
      calls.push_back({{"lastLines",sample.lastLines},{"allLines",sample.allLines},
        {"totalLines",sample.totalLines},{"totalBytes",sample.totalBytes},
        {"isIncomplete",sample.isIncomplete}});
    }
    output.push_back(std::move(calls));
  }
  std::cout<<batbox::value::stringify(output)<<'\n';
}
