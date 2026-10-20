#pragma once

#include <stdexcept>

namespace core {

class ConflictError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NotFoundError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace core
