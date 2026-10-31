#include "terminal_ftxui/Driver.hpp"
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <string>
#include <vector>
using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::vector<std::string> rendered{"first\n","second\n","third\n"};
  std::size_t next=0;
  std::string output;
  int resized=0,exited=0;
  terminal_ftxui::Options options;
  options.interactive=false;
  options.redirectedWidth=42;
  options.redirectedHeight=11;
  terminal_ftxui::Callbacks callbacks;
  callbacks.render=[]{return ftxui::text("unused interactive view");};
  callbacks.onRawInputBytes=[](std::string_view){};
  callbacks.onResize=[&](ftxui::Dimensions size){
    if(size.dimx!=42||size.dimy!=11)throw std::runtime_error("bad size");
    ++resized;
  };
  callbacks.renderPlain=[&]{return next<rendered.size()?rendered[next++]:"";};
  callbacks.writePlain=[&](std::string_view text){output+=text;};
  callbacks.onExit=[&]{++exited;};
  auto driver=std::make_shared<terminal_ftxui::Driver>(loop,options,callbacks);
  driver->start();
  driver->requestRedraw();
  driver->setMode(terminal_ftxui::Mode::Fullscreen);
  driver->stop();
  driver->requestRedraw();
  if(output!="first\nsecond\nthird\n"||resized!=2||exited!=1||
     driver->running()||driver->interactive()||ftxui::App::Active()!=nullptr)
    return 1;
  std::cout<<"FTXUI redirected driver: plain incremental output, mode and exit pass\n";
}
