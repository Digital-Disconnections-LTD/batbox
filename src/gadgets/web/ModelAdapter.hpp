#pragma once
#include "model/QueryModel.hpp"
#include "messages/Messages.hpp"
#include "gadgets/web/Web.hpp"

namespace batbox::gadgets::web {
using QueryServicesFactory=std::function<model::QueryServices()>;
// Bind the small-model query used to process fetched web content.
std::function<Json(const Json&,std::stop_token)> bindSmallModel(
  QueryServicesFactory makeQueryServices,messages::Services messageServices,
  std::function<std::string()> getSmallFastModel);
} // namespace batbox::gadgets::web
