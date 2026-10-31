#include "termio/Output.hpp"
#include "termio/Dec.hpp"
#include "fs_operations/IndigenousWritableStream.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
int main(){using namespace batbox;
  const auto path=std::filesystem::temp_directory_path()/
    ("batbox-terminal-output-"+std::to_string(::getpid()));
  event_loop::Loop loop;
  auto host=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,1);
  auto stream=fs_operations::createIndigenousWritableStream(loop,host,path.string());
  bool finished=false;stream->onFinish([&]{finished=true;});
  auto services=termio::indigenousOutputServices(termio::indigenousTerminalEnvironment(),[]{return "kitty";});
  const auto write=termio::bindTerminalWriter(stream);
  termio::writeDiffToTerminal(write,Json::array({Json{{"type","stdout"},{"content","hello\n"}},
    Json{{"type","cursorMove"},{"x",-2},{"y",1}}}),false,services);
  stream->end();loop.run();assert(finished);
  std::ifstream input(path,std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(input)),{});
  assert(bytes==termio::BSU+"hello\n\x1b[2D\x1b[1B"+termio::ESU);
  std::filesystem::remove(path);
}
