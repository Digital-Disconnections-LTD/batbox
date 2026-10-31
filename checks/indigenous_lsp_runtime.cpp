#include "application_lsp/Runtime.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
  assert(argc==2);
  namespace fs=std::filesystem;
  using batbox::Json;
  auto root=fs::temp_directory_path()/"batbox-lsp-check";
  fs::create_directories(root);
  auto file=root/"source λ.cpp";
  {std::ofstream out(file);out<<"int value = 1;\n";}
  Json servers={{"servers",{{"fixture",{{"command","python3"},
    {"args",Json::array({argv[1]})},
    {"extensionToLanguage",{{".cpp","cpp"}}}}}}}};
  auto runtime=batbox::application_lsp::Runtime::create(root,servers);
  assert(runtime->available()&&runtime->supportsFile(file.string()));
  batbox::gadgets::GadgetCallContext context;
  Json definition={{"operation","goToDefinition"},{"filePath",file.string()},
    {"line",1},{"character",5}};
  auto result=runtime->callGadget(definition,context).get().data;
  assert(result["resultCount"]==1&&result["fileCount"]==1);
  assert(result["result"].get<std::string>().find("source λ.cpp:1:5")!=std::string::npos);
  auto diagnostics=runtime->takeDiagnosticSets();
  assert(diagnostics.size()==1&&diagnostics[0]["files"][0]["diagnostics"].size()==1);
  runtime->changeFile(file.string(),"int value = 2;\n");
  runtime->saveFile(file.string());
  definition["operation"]="hover";
  auto hover=runtime->callGadget(definition,context).get().data;
  assert(hover["result"]=="fixture hover");
  runtime->clearDeliveredDiagnostics(batbox::lsp::fileUri(file));
  assert(runtime->takeDiagnosticSets().size()==1);
  runtime->shutdownAsync().get();
  fs::remove_all(root);
  std::cout<<"native LSP runtime check passed\n";
}
