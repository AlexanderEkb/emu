#include "host.h"
#include <Arduino.h>
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

  ESP_LOGI(TAG, "Hello! We're starting with %i bytes in the heap", ESP.getFreeHeap());
  ESP_LOGI(TAG, "Initializing video");
  video->init();
  // ESP_LOGI(TAG, "Initializing keyboard");
  // keyboard->init();
  // ESP_LOGI(TAG, "Initializing storage");
  // storage->init();

  // Enumerate available machines
  // If only one is available, run it automatically
}

void Host_t::run()
{
  while(1);
}

void Host_t::shutdown()
{

}
