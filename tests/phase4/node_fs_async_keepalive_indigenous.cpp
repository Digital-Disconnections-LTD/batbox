#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

int main(){
  using namespace std::chrono_literals;
  batbox::event_loop::Loop loop;
  auto host=std::make_shared<batbox::fs_operations::IndigenousAsyncFsHost>(loop,1);
  bool fulfilled=false,rejected=false;
  auto start=std::chrono::steady_clock::now();
  auto first=host->submit<int>([]{std::this_thread::sleep_for(45ms);return 7;});
  first.then([&](const int* value,const std::exception_ptr& error){
    assert(!error&&value&&*value==7);fulfilled=true;});
  loop.run();
  assert(fulfilled);
  assert(std::chrono::steady_clock::now()-start>=35ms);
  auto second=host->submit<int>([]{std::this_thread::sleep_for(20ms);
    throw std::runtime_error("worker failure");return 0;});
  second.then([&](const int*,const std::exception_ptr& error){
    assert(error);rejected=true;});
  loop.run();
  assert(rejected);
  host.reset();
}
