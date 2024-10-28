#ifndef __HOST_HOST_H__
#define __HOST_HOST_H__

#include "video/video.h"
#include "input/keyboard.h"
#include "storage/storage.h"

#define EMU_HOST_VERSION  100

class Host_t
{
  public:
    Video_t    *video;
    Keyboard_t *keyboard;
    Storage_t  *storage;

    void init();
    void run();
    void shutdown();
  private:
};

#endif /* __HOST_HOST_H__ */