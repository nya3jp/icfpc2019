#include "examples/minimal_lib.h"
#include "gtest/gtest.h"

namespace {

class MinimalTest : public testing::Test {
};

TEST_F(MinimalTest, BuildHelloMessage) {
  EXPECT_EQ("Hello, world!", BuildHelloMessage());
}

}  // namespace
