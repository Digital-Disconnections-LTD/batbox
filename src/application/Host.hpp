#pragma once
#include "application/Config.hpp"
#include <memory>

namespace batbox::application {
class Host final {
public:
  explicit Host(Config config);
  ~Host();
  Host(const Host&)=delete;
  Host& operator=(const Host&)=delete;
  int run();
private:
  friend struct HostLifetimeProbe;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
