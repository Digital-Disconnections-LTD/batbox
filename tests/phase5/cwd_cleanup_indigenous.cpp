#include "cwd/Override.hpp"
#include <cassert>
#include <string>

using namespace batbox;
int main(){
  const auto base=[]{return std::string("/physical");};
  auto wrapped=cwd::runWithCwdOverride("/link",[&]{
    return cwd::bindCurrentCwd([&]{return cwd::pwd(base);});
  });
  assert(cwd::pwd(base)=="/physical");
  assert(wrapped()=="/link");
  assert(cwd::pwd(base)=="/physical");
  assert(cwd::runWithCwdOverride("/inner",[&]{return cwd::pwd(base);})=="/inner");
}
