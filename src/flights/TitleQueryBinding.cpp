#include "flights/TitleQueryBinding.hpp"
namespace batbox::flights {
TitleServices bindTitleQuery(event_loop::Loop& loop,TitleServices services,
  model::SimpleQueryServices query){
  services.querySmallFast=[&loop,query=std::move(query)](TitleQuery input){
    model::SimpleQueryInputs request;
    request.systemPoetry=std::move(input.systemPoetry);
    request.userPoetry=std::move(input.userPoetry);
    request.outputFormat=std::move(input.outputFormat);
    request.options=std::move(input.options);request.signal=input.signal;
    return model::querySmallFast(loop,std::move(request),query);
  };
  return services;
}
}
