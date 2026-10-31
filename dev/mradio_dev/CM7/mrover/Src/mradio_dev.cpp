#include "main.h"
#include "web.hpp"

namespace mrover {
void init() { web::init(); }
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
