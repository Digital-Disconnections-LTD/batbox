#include "terminal_runtime_roster/Instances.hpp"
#include <iostream>
#include <string>

using namespace batbox;
struct Stream{};struct Instance{};
int main(){
  terminal_runtime_roster::Instances<Stream,Instance> roster;
  const auto stream=std::make_shared<Stream>(),
    otherStream=std::make_shared<Stream>();
  const auto first=std::make_shared<Instance>(),
    second=std::make_shared<Instance>();
  std::cout<<"[";
  bool comma=false;
  const auto row=[&](const char* step,bool result){
    if(comma)std::cout<<",";comma=true;
    const auto current=roster.get(stream);
    std::cout<<"{\"step\":\""<<step<<"\",\"result\":"
      <<(result?"true":"false")<<",\"size\":"<<roster.size()
      <<",\"current\":";
    if(current==first)std::cout<<"\"first\"";
    else if(current==second)std::cout<<"\"second\"";
    else std::cout<<"null";
    std::cout<<"}";
  };
  row("empty",!roster.get(stream));
  row("setFirst",&roster.set(stream,first)==&roster);
  row("sameKey",roster.get(stream)==first);
  row("otherKey",!roster.get(otherStream));
  row("replace",&roster.set(stream,second)==&roster);
  row("replaced",roster.get(stream)==second);
  row("eraseMissing",roster.erase(otherStream));
  row("erase",roster.erase(stream));
  row("eraseAgain",roster.erase(stream));
  std::cout<<"]\n";
}
