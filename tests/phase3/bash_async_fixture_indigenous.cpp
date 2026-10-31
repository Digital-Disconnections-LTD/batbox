#include <cstdlib>
#include "BashAsyncFixture.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>

int main(){
  char temporary[]="/tmp/batbox-bash-fixture-XXXXXX";assert(::mkdtemp(temporary));
  const std::filesystem::path root=temporary;
  try{
    batbox::event_loop::Loop loop;
    auto fixture=batbox::test_support::BashAsyncFixture::create(loop,root);
    batbox::gadgets::GadgetCallContext context;context.data=batbox::Json::object();
    auto pending=fixture->gadget.call({{"command","printf fixture"}},context,{},batbox::Json(),{});
    loop.run();assert(pending.get().data["stdout"]=="fixture");
    fixture.reset();std::filesystem::remove_all(root);
    std::cout<<"shared real Bash async test fixture passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
