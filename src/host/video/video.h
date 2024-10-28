#ifndef __HOST_VIDEO_H__
#define __HOST_VIDEO_H__

#include <stdint.h>

/*
 * The goal is to support at least two kinds of video outputs: VGA (or similar) and composite video.
 * Having a possibility to connect TFT panels is nice to have, but optional.
*/
typedef enum PixelFormat_t
{
  PIXEL_FORMAT_I8,      // 8-bit indexed
  PIXEL_FORMAT_RGB222,  // Simple VGA implementation
  PIXEL_FORMAT_RGB565   // Most popular format for cheap widespread TFTs
};

typedef struct VideoModeDesc_t
{
  uint32_t width;
  uint32_t height;
  PixelFormat_t pixelFormat;
  uint32_t frameRate;
  uint32_t flags;
} VideoModeDesc_t;

class Video_t
{
  public:
    virtual bool init();
    virtual uint32_t getModesCount();
    virtual bool getModeDesc(uint32_t mode, VideoModeDesc_t & buffer);

    virtual uint32_t getWidth();
    virtual uint32_t getHeight();
    virtual PixelFormat_t getPixelFormat();
    
    virtual void * getScanline(uint32_t line);
};

#endif /* __HOST_VIDEO_H__ */