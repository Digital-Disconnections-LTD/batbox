#define main flights_fixture_main
#include "flights_probe.cpp"
#undef main
int main(){std::string line;while(std::getline(std::cin,line))try{
  auto input=value::parse(line);input["asyncLoad"]=true;
  std::cout<<value::stringify(Json{{"result",evaluate(input)}})<<'\n';
}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}}
