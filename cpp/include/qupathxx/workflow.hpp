#pragma once
#include <stdexcept>
namespace qpx {
class WorkflowInputChanged : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
}
