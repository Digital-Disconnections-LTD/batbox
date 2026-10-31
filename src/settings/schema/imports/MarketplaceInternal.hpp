#pragma once
#include "settings/schema/imports/Marketplace.hpp"
#include "settings/schema/Runtime.hpp"

namespace batbox::settings::schema::imports::detail {
void applyMarketplaceChecks(const Json& node,const Json& input,
  const Json& path,Json& output,Json& issues,
  const UrlCanonicalizer& canonicalUrl);
schema::detail::ParseResult parseMarketplaceNode(const Json& node,
  const Json& input,bool present,const Json& path,
  const MainImports& imports);
}
