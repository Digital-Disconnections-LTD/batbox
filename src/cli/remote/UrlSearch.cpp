#include "cli/remote/UrlSearch.hpp"
#include "runtime/Encoding.hpp"
#include <ada.h>
#include <stdexcept>

namespace batbox::cli::remote {
std::string setUrlSearchParam(const std::string& url,
    const std::string& key,const std::string& value){
  auto parsed=ada::parse<ada::url_aggregator>(value::encodeUtf8(url));
  if(!parsed)throw std::invalid_argument("Invalid URL");
  ada::url_search_params params(parsed->get_search());
  params.set(value::encodeUtf8(key),value::encodeUtf8(value));
  parsed->set_search(params.to_string());
  return std::string(parsed->get_href());
}
}
