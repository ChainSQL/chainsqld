#pragma once

#include <stdint.h>

namespace chainsql {
  inline void check(bool test, const void* msg, uint32_t msg_len) {
  }

  inline void check(bool test, const void* msg) {
  }
}
