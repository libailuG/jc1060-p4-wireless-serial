#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Hosted 2.5.2 introduced controller lifecycle RPC and disabled automatic
 * controller startup. Earlier firmware exposes an already enabled HCI pipe. */
static inline bool hosted_ble_controller_rpc(uint32_t major, uint32_t minor, uint32_t patch)
{
    return major > 2 || (major == 2 && (minor > 5 || (minor == 5 && patch >= 2)));
}
