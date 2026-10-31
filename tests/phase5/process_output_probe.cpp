#include "cli/process_output/ProcessOutput.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    event_loop::Loop loop;
    auto workers=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,1);
    int out[2],err[2];
    if(::pipe(out)||::pipe(err))throw std::runtime_error("pipe failed");
    cli::process_output::ProcessOutput output(loop,workers,out[1],err[1]);
    bool outBroken=false,errBroken=false;
    std::string stdoutBytes,stderrBytes;
    Json errors=Json::array();
    for(const auto& action:input.at("actions")){
      const auto kind=action.at(0).get<std::string>();
      try{
        if(kind=="register")output.registerProcessOutputErrorHandlers();
        else if(kind=="break-out"){
          ::close(out[0]);outBroken=true;
        }else if(kind=="break-err"){
          ::close(err[0]);errBroken=true;
        }else if(kind=="out"||kind=="err"){
          const auto data=action.at(1).get<std::string>();
          if(kind=="out")output.writeToStdout(data);
          else output.writeToStderr(data);
          loop.run();
          const int fd=kind=="out"?out[0]:err[0];
          const bool broken=kind=="out"?outBroken:errBroken;
          const bool destroyed=kind=="out"?output.stdoutDestroyed():
            output.stderrDestroyed();
          if(!broken&&!destroyed&&!data.empty()){
            std::string read(data.size(),'\0');
            const auto count=::read(fd,read.data(),read.size());
            if(count<0)throw std::runtime_error("read failed");
            read.resize(static_cast<std::size_t>(count));
            if(kind=="out")stdoutBytes+=read;
            else stderrBytes+=read;
          }
        }
      }catch(const std::exception&){errors.push_back(kind);}
    }
    if(!outBroken)::close(out[0]);
    if(!errBroken)::close(err[0]);
    ::close(out[1]);::close(err[1]);
    std::cout<<value::stringify(Json{{"stdout",stdoutBytes},
      {"stderr",stderrBytes},
      {"stdoutDestroyed",output.stdoutDestroyed()},
      {"stderrDestroyed",output.stderrDestroyed()},
      {"errors",errors}})<<'\n';
  }
}
