#include "examples/minimal_lib.h"

#include "absl/strings/str_format.h"

std::string BuildHelloMessage() {
  return absl::StrFormat("Hello, %s!", "world");
}
