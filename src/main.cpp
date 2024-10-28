#include <Arduino.h>
#include "host/host.h"

#if (EMU_HOST_VERSION < 100)
#error "Host is too old, please update"
#endif

Host_t host;

void setup() {
  host.init();
}

void loop() {
  // put your main code here, to run repeatedly:
}
