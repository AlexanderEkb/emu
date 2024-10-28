#include "host.h"
#include <esp32/spiram.h>
#include <esp32-hal.h>

#define TAG "host"

void Host_t::init()
{
  disableCore0WDT();
  delay(100);
  disableCore1WDT();

  ESP_ERROR_CHECK(esp_spiram_init());
  esp_spiram_init_cache();

  video->init();
  keyboard->init();
  storage->init();

  // Enumerate available machines
  // If only one is available, run it automatically
}