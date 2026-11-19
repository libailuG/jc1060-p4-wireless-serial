#include "app_registry.h"

/* Keep sample first: existing boot_sample configuration retains its meaning. */
const app_descriptor_t *const app_registry[] = {&sample_app, &clock_app, &counter_app, &weather_app, &serial_app, &xiaozhi_app, &keys_app, &bridge_app, &wireless_app};
const size_t app_registry_count = sizeof(app_registry) / sizeof(app_registry[0]);





