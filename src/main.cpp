#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portable.h>
#include <FreeRTOSConfig.h>

#include "host/host.h"
#include "host/video/NTSC/videoNTSC.h"

#if (EMU_HOST_VERSION < 100)
#error "Host is too old, please update"
#endif

VideoNTSC_t video;
Host_t host = Host_t(&video);

void setup() {
  host.init();
}

void loop() {
  host.run();
}
