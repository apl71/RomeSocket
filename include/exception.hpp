#ifndef EXCEPTION_HPP_
#define EXCEPTION_HPP_

#include <exception>

class SocketException : public std::exception {
public:
    const char *what() const noexcept override { return "Fail to Create Socket"; }
};

#endif