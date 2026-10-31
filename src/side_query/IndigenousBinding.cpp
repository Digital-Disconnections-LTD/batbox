#include "side_query/IndigenousBinding.hpp"
#include "transport/AsyncMessages.hpp"

namespace batbox::side_query {
void bindIndigenousClientFactory(event_loop::Loop& loop,Services& target,
    client_factory::Services factory,transport::SdkRetryClock clock){
  target.getClient=[&loop,factory=std::move(factory),clock=std::move(clock)](
      ClientOptions options){
    event_loop::Deferred<std::shared_ptr<Client>> output(loop);
    try{
      client_factory::Inputs input;
      input.model=options.model;input.source=options.source;
      // Retry belongs to the indigenous asynchronous SDK adapter. The raw HTTP
      // client performs exactly one attempt per adapter request.
      input.maxRetries=0;
      auto pending=client_factory::getBatboxClient(loop,std::move(input),factory);
      pending.then([&loop,output,options,clock](
          const std::shared_ptr<transport::Client>* indigenous,
          const std::exception_ptr& error){
        try{
          if(error)std::rethrow_exception(error);
          auto client=std::make_shared<Client>();
          client->create=[&loop,indigenous=*indigenous,retries=options.maxRetries,clock](
              Json body,std::stop_token signal){
            event_loop::Deferred<ResponseRef> result(loop);
            try{
              auto pending=transport::createMessage(loop,indigenous,std::move(body),
                retries,signal,clock);
              pending.then([result](const transport::Response* value,
                  const std::exception_ptr& failure){
                if(failure){result.reject(failure);return;}
                result.resolve(std::make_shared<Json>(value->body));
              });
            }catch(...){result.reject(std::current_exception());}
            return result;
          };
          output.resolve(std::move(client));
        }catch(...){output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
} // namespace batbox::side_query
