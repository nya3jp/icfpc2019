#include "examples/data.pb.h"

int main(int argc, char** argv) {
  examples::Data data;
  data.set_msg("foo");
  return 0;
}
