#pragma once
namespace batbox::instructions::project {struct DiscoveryServices;}
namespace batbox::glob {
// Supplies the two concrete matcher imports used by instruction discovery.
void bindDiscoveryMatchers(instructions::project::DiscoveryServices& services);
} // namespace batbox::glob
