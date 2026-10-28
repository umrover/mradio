#include "main.h"

namespace mrover {
void init() {}
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
