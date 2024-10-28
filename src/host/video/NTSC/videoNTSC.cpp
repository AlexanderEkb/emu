#include <string.h>
#include "videoNTSC.h"

#include "esp_types.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/ledc_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/io_mux_reg.h"
#include "rom/gpio.h"
#include "rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"

#define TAG "video_NTSC"

constexpr uint16_t VideoNTSC_t::NTSC_BURST_4[12];
constexpr VideoModeDesc_t VideoNTSC_t::modes[VideoNTSC_t::NUMBER_OF_MODES];
constexpr DRAM_ATTR uint32_t VideoNTSC_t::atari_4_phase_ntsc[256];
constexpr DRAM_ATTR uint32_t VideoNTSC_t::atari_4_phase_ntsc_even[256];
constexpr DRAM_ATTR uint16_t VideoNTSC_t::atari_4_phase_ntsc_odd[256];

void IRAM_ATTR i2s_intr_handler_video(void *arg);

VideoNTSC_t::VideoNTSC_t()
{
  _line_counter = 0;
  _frame_counter = 0;
  _phase = 0;

  mode = 0;
  colorburstEnabled = true;
  _blitter = &VideoNTSC_t::blitter_0;
}

bool VideoNTSC_t::init()
{
  framebuffer = (uint8_t **)malloc(NATIVE_HEIGHT * sizeof(char *));
  assert(framebuffer);
  for (int y = 0; y < NATIVE_HEIGHT; y++)
  {
    framebuffer[y] = (uint8_t *)malloc(NATIVE_WIDTH * 2);
    if(framebuffer[y] == nullptr)
    {
      ESP_LOGE(TAG, "Error allocating line %i", y);
      assert(framebuffer[y]);
    }
    // memset(framebuffer[y], 0x00, NATIVE_WIDTH * 2);
  }

  periph_module_enable(PERIPH_I2S0_MODULE);

  // setup interrupt
  if (esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_INTRDISABLED,
    i2s_intr_handler_video, this, &_isr_handle) != ESP_OK)
    return -1;

  // reset conf
  I2S0.conf.val = 1;
  I2S0.conf.val = 0;
  I2S0.conf.tx_right_first = 1;
  I2S0.conf.tx_mono = 1;

  I2S0.conf2.lcd_en = 1;
  I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
  I2S0.sample_rate_conf.tx_bits_mod = 16;
  I2S0.conf_chan.tx_chan_mod = 1;

  // Create TX DMA buffers
  for (int i = 0; i < 2; i++) {
    int n = _line_width*2;
    if (n >= 4092) {
      printf("DMA chunk too big: %i\n",n);
      return -1;
    }
    _dma_desc[i].buf = (uint8_t*)heap_caps_calloc(1, n, MALLOC_CAP_DMA);
    if (!_dma_desc[i].buf)
      return -1;
    
    _dma_desc[i].owner = 1;
    _dma_desc[i].eof = 1;
    _dma_desc[i].length = n;
    _dma_desc[i].size = n;
    _dma_desc[i].empty = (uint32_t)(i == 1 ? _dma_desc : _dma_desc+1);
  }
  I2S0.out_link.addr = (uint32_t)_dma_desc;

  //  Setup up the apll: See ref 3.2.7 Audio PLL
  //  f_xtal = (int)rtc_clk_xtal_freq_get() * 1000000;
  //  f_out = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536); // 250 < f_out < 500
  //  apll_freq = f_out/((o_div + 2) * 2)
  //  operating range of the f_out is 250 MHz ~ 500 MHz
  //  operating range of the apll_freq is 16 ~ 128 MHz.
  //  select sdm0,sdm1,sdm2 to produce nice multiples of colorburst frequencies

  //  see calc_freq() for math: (4+a)*10/((2 + b)*2) mhz
  //  up to 20mhz seems to work ok:
  //  rtc_clk_apll_enable(1,0x00,0x00,0x4,0);   // 20mhz for fancy DDS

  switch (_samples_per_cc) {
    case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // 10.7386363636 3x NTSC (10.7386398315mhz)
    case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // 14.3181818182 4x NTSC (14.3181864421mhz)
  }
  I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider’s integral value.
  I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider’s numerator value.
  I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider’s denominator value
  I2S0.sample_rate_conf.tx_bck_div_num = 1;
  I2S0.clkm_conf.clka_en = 1;                 // Set this bit to enable clk_apll.
  I2S0.fifo_conf.tx_fifo_mod = 1;             // 32-bit dual or 16-bit single channel data

  dac_output_enable(DAC_CHANNEL_1);           // DAC, video on GPIO25
  dac_i2s_enable();                           // start DAC!

  I2S0.conf.tx_start = 1;                     // start DMA!
  I2S0.int_clr.val = 0xFFFFFFFF;
  I2S0.int_ena.out_eof = 1;
  I2S0.out_link.start = 1;
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_intr_enable(_isr_handle));  // start interruprs!
}

uint32_t VideoNTSC_t::getModesCount()
{
  return NUMBER_OF_MODES;
}

VideoModeDesc_t const * VideoNTSC_t::getModeDesc(uint32_t mode)
{
  return &modes[mode];
}

uint32_t VideoNTSC_t::getWidth()
{
  return modes[mode].width;
}

uint32_t VideoNTSC_t::getHeight()
{
  return modes[mode].height;
}

PixelFormat_t VideoNTSC_t::getPixelFormat()
{
  return modes[mode].pixelFormat;
}

void VideoNTSC_t::setVideoMode(uint32_t mode)
{

}

void * VideoNTSC_t::getScanline(uint32_t line)
{
  
}

void __attribute__((optimize("-Ofast"))) IRAM_ATTR VideoNTSC_t::sync(uint16_t *line, int syncwidth)
{
  for (int i = 0; i < syncwidth; i++)
    line[i] = SYNC_LEVEL;
}

void __attribute__((optimize("-Ofast"))) IRAM_ATTR VideoNTSC_t::burst(uint16_t* line)
{
    int i,phase;
    switch (_samples_per_cc) {
        case 4:
            // 4 samples per color clock
            for (i = _hsync; i < _hsync + (4*10); i += 4) {
                if (colorburstEnabled)
                {
                    line[i + 0] = NTSC_BURST_4[_phase + 0];
                    line[i + 1] = NTSC_BURST_4[_phase + 1];
                    line[i + 2] = NTSC_BURST_4[_phase + 2];
                    line[i + 3] = NTSC_BURST_4[_phase + 3];
                }
                else
                {
                    line[i + 0] = BLANKING_LEVEL;
                    line[i + 1] = BLANKING_LEVEL;
                    line[i + 2] = BLANKING_LEVEL;
                    line[i + 3] = BLANKING_LEVEL;
                }
            }
            break;
        case 3:
            // 3 samples per color clock
            phase = 0.866025*BLANKING_LEVEL/2;
            for (i = _hsync; i < _hsync + (3*10); i += 6) {
                line[i+1] = BLANKING_LEVEL;
                line[i+0] = BLANKING_LEVEL + phase;
                line[i+3] = BLANKING_LEVEL - phase;
                line[i+2] = BLANKING_LEVEL;
                line[i+5] = BLANKING_LEVEL + phase;
                line[i+4] = BLANKING_LEVEL - phase;
            }
            break;
        }
}

void __attribute__((optimize("-Ofast"))) IRAM_ATTR VideoNTSC_t::blank(uint16_t *line, int syncwidth)
{
  for (int i = syncwidth; i < _line_width; i++)
    line[i] = BLANKING_LEVEL;
}

void VideoNTSC_t::video_isr(volatile void* vbuf)
{
    int i = _line_counter++;
    uint16_t* buf = (uint16_t*)vbuf;
    // ntsc
    if (i < NATIVE_HEIGHT) {                // active video
      sync(buf,_hsync);
      burst(buf);
      ((*this).*(_blitter))(framebuffer[i], buf + _active_start);
    } else  {
      if (i < (NATIVE_HEIGHT + 5)) {   // post render/black
        sync(buf, _hsync);
        blank(buf, _hsync);
        burst(buf);
      } else if (i < (NATIVE_HEIGHT + 8)) {   // vsync
        sync(buf, _hsync_long);
        blank(buf, _hsync_long);
      } else { // pre render/black
        sync(buf, _hsync);
        blank(buf, _hsync);
        burst(buf);
      }
    }

    if (_line_counter == NTSC_LINES) {
        _line_counter = 0;                      // frame is done
        _frame_counter++;
    }
  }

/// @brief So-called "hi-res" blitter, handling modes with 640 pixels per line
/// @param src pointer to a line in the framebuffer
/// @param dst DMA buffer
/// @return 
void __attribute__((optimize("-Ofast"))) IRAM_ATTR VideoNTSC_t::blitter_0(uint8_t *src, uint16_t *dst)
{
  static const uint32_t STEP = 4;
  static const uint32_t MASK_EVEN = 0x0000FFFF;
  static const uint32_t MASK_ODD = 0xFFFF0000;

  uint32_t *d = (uint32_t *)(dst + 32);
  for (int i = 0; i < NATIVE_WIDTH; i += STEP) // 84 steps, 4 pixels per step
  {
    d[0] = (atari_4_phase_ntsc_even[src[0]] | atari_4_phase_ntsc_odd[src[1]]);
    d[1] = (atari_4_phase_ntsc_even[src[2]] | atari_4_phase_ntsc_odd[src[3]]) << 8;
    d[2] = (atari_4_phase_ntsc_even[src[4]] | atari_4_phase_ntsc_odd[src[5]]);
    d[3] = (atari_4_phase_ntsc_even[src[6]] | atari_4_phase_ntsc_odd[src[7]]) << 8;
    d += STEP;
    src += STEP << 1;
  }
}

/// @brief So-called "lo-res" blitter, handling modes with 320 pixels per line
/// @param src pointer to a line in the framebuffer
/// @param dst DMA buffer
/// @return 
void __attribute__((optimize("-Ofast"))) IRAM_ATTR VideoNTSC_t::blitter_1(uint8_t *src, uint16_t *dst)
{
  static const uint32_t STEP = 4;

  uint32_t *d = (uint32_t *)(dst + 32);
  for (int i = 0; i < NATIVE_WIDTH; i += STEP) // 84 steps, 4 pixels per step
  {
    d[0] = atari_4_phase_ntsc[src[0]] << 0;
    d[1] = atari_4_phase_ntsc[src[1]] << 8;
    d[2] = atari_4_phase_ntsc[src[2]] << 0;
    d[3] = atari_4_phase_ntsc[src[3]] << 8;
    d += STEP;
    src += STEP;
  }
}

// simple isr
void IRAM_ATTR i2s_intr_handler_video(void *arg) {
    if (I2S0.int_st.out_eof)
      reinterpret_cast<VideoNTSC_t *>(arg)->video_isr(((lldesc_t*)I2S0.out_eof_des_addr)->buf); // get the next line of video
    I2S0.int_clr.val = I2S0.int_st.val;                     // reset the interrupt
}
