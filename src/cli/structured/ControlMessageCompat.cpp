#include "cli/structured/ControlMessageCompat.hpp"

namespace batbox::cli::structured {
namespace {
void normalizeRecord(Json& record){
  if(!record.is_object()||!record.contains("requestId")||
     record.contains("request_id"))return;
  record["request_id"]=record["requestId"];
  record.erase("requestId");
}
}
void normalizeControlMessageKeys(Json& object){
  if(!object.is_object())return;
  normalizeRecord(object);
  const auto found=object.find("response");
  if(found!=object.end()&&found->is_object())normalizeRecord(*found);
}
}
