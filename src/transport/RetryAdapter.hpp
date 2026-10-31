#pragma once
#include "api_errors/ApiErrors.hpp"
#include "retry/Retry.hpp"
#include <exception>
namespace batbox::transport {
retry::ErrorRecord errorRecordFromException(std::exception_ptr failure);
api_errors::Failure apiFailureFromException(std::exception_ptr failure);
} // namespace batbox::transport
