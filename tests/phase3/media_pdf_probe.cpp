#include "media/Media.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto path=input.at("path").get<std::string>();
    Json output;
    if(input.at("op")=="read")output=batbox::media::readPDF(path);
    else if(input.at("op")=="count"){
      const auto count=batbox::media::getPDFPageCount(path);
      output=count?Json(*count):Json();
    }else if(input.at("op")=="extract"){
      batbox::media::Config config;
      const auto directory=input.at("outdir").get<std::string>();
      config.getGadgetResultsDir=[directory](){return directory;};
      config.randomUUID=[](){return std::string("fixture");};
      std::optional<batbox::gadgets::files::PdfPageRange> range;
      if(input.contains("pages")){
        const auto pages=input.at("pages");
        range=batbox::gadgets::files::PdfPageRange{
          pages.value("firstPage",0.0),pages.value("lastPage",0.0)};
      }
      output=batbox::media::extractPDFPages(path,range,config);
    }else throw std::runtime_error("unknown operation");
    std::cout<<batbox::value::stringify(Json{{"result",output}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}
