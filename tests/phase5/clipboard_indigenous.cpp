#include "termio/Clipboard.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
int main(){using namespace batbox;
  const auto base=std::filesystem::temp_directory_path()/
    ("batbox-clipboard-native-"+std::to_string(::getpid()));
  std::filesystem::create_directories(base);
  const auto command=base/"copy-fixture",destination=base/"copied.txt";
  {std::ofstream script(command);script<<"#!/bin/sh\ncat > '"<<destination.string()<<"'\n";}
  ::chmod(command.c_str(),0700);
  event_loop::Loop loop;
  auto indigenousExec=process::indigenousExeca(loop);process::ExecImports imports;
  imports.execa=[&,indigenousExec](process::ExecaRequest request){
    assert(request.file=="wl-copy"&&request.args.empty());
    assert(!request.options.cwd&&request.options.timeout==2000);
    request.file=command.string();return indigenousExec(std::move(request));
  };
  imports.logError=[](std::exception_ptr){assert(false);};
  termio::TerminalEnvironment environment{"linux",[](const std::string&)->std::optional<std::string>{return {};}};
  termio::ClipboardModule module(loop,termio::indigenousClipboardServices(loop,environment,[]{return "kitty";},imports));
  bool done=false;
  module.setClipboard("native input").then([&](const std::string* value,const std::exception_ptr& error){
    assert(!error&&value&&*value=="\x1b]52;c;bmF0aXZlIGlucHV0\x1b\\");done=true;
  });
  loop.run();assert(done);
  std::ifstream file(destination);const std::string text((std::istreambuf_iterator<char>(file)),{});
  assert(text=="native input");std::filesystem::remove_all(base);
}
