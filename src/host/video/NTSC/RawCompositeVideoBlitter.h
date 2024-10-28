
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

/**
 * Change log:
 *
 * Oct 2021 - Original code from https://github.com/rossumur/esp_8_bit
 *            Modified by Marcio Teixeira to extract Atari framebuffer
 *            and make it compatible with Bitluni's code. I don't
 *            really understand this code very well, but there are
 *            some implementation details in the esp_8_bit repo.
 */

#pragma once
#define VIDEO_PIN         26
#define DOUBLE_PIXEL_RATE 1

#include "esp_types.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
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

namespace RawCompositeVideoBlitter {

typedef void (*blitter_t)(uint8_t *src, uint16_t *dst);
blitter_t _blitter;

///  Always 0, consider removing this
uint32_t _phase = 0;

//====================================================================================================
//====================================================================================================
// Color palettes
//

const static uint32_t atari_palette_rgb[256] = {
    0x00000000,0x000F0F0F,0x001B1B1B,0x00272727,0x00333333,0x00414141,0x004F4F4F,0x005E5E5E,
    0x00686868,0x00787878,0x00898989,0x009A9A9A,0x00ABABAB,0x00BFBFBF,0x00D3D3D3,0x00EAEAEA,
    0x00001600,0x000F2100,0x001A2D00,0x00273900,0x00334500,0x00405300,0x004F6100,0x005D7000,
    0x00687A00,0x00778A17,0x00899B29,0x009AAC3B,0x00ABBD4C,0x00BED160,0x00D2E574,0x00E9FC8B,
    0x001C0000,0x00271300,0x00331F00,0x003F2B00,0x004B3700,0x00594500,0x00675300,0x00756100,
    0x00806C12,0x008F7C22,0x00A18D34,0x00B29E45,0x00C3AF56,0x00D6C36A,0x00EAD77E,0x00FFEE96,
    0x002F0000,0x003A0000,0x00460F00,0x00521C00,0x005E2800,0x006C3600,0x007A4416,0x00885224,
    0x00925D2F,0x00A26D3F,0x00B37E50,0x00C48F62,0x00D6A073,0x00E9B487,0x00FDC89B,0x00FFDFB2,
    0x00390000,0x00440000,0x0050000A,0x005C0F17,0x00681B23,0x00752931,0x0084373F,0x0092464E,
    0x009C5058,0x00AC6068,0x00BD7179,0x00CE838A,0x00DF949C,0x00F2A7AF,0x00FFBBC3,0x00FFD2DA,
    0x00370020,0x0043002C,0x004E0037,0x005A0044,0x00661350,0x0074215D,0x0082306C,0x00903E7A,
    0x009B4984,0x00AA5994,0x00BC6AA5,0x00CD7BB6,0x00DE8CC7,0x00F1A0DB,0x00FFB4EF,0x00FFCBFF,
    0x002B0047,0x00360052,0x0042005E,0x004E006A,0x005A1276,0x00672083,0x00762F92,0x00843DA0,
    0x008E48AA,0x009E58BA,0x00AF69CB,0x00C07ADC,0x00D18CED,0x00E59FFF,0x00F9B3FF,0x00FFCAFF,
    0x0016005F,0x0021006A,0x002D0076,0x00390C82,0x0045198D,0x0053279B,0x006135A9,0x006F44B7,
    0x007A4EC2,0x008A5ED1,0x009B6FE2,0x00AC81F3,0x00BD92FF,0x00D0A5FF,0x00E4B9FF,0x00FBD0FF,
    0x00000063,0x0000006F,0x00140C7A,0x00201886,0x002C2592,0x003A329F,0x004841AE,0x00574FBC,
    0x00615AC6,0x00716AD6,0x00827BE7,0x00948CF8,0x00A59DFF,0x00B8B1FF,0x00CCC5FF,0x00E3DCFF,
    0x00000054,0x00000F5F,0x00001B6A,0x00002776,0x00153382,0x00234190,0x0031509E,0x00405EAC,
    0x004A68B6,0x005A78C6,0x006B89D7,0x007D9BE8,0x008EACF9,0x00A1BFFF,0x00B5D3FF,0x00CCEAFF,
    0x00001332,0x00001E3E,0x00002A49,0x00003655,0x00004261,0x0012506F,0x00205E7D,0x002F6D8B,
    0x00397796,0x004987A6,0x005B98B7,0x006CA9C8,0x007DBAD9,0x0091CEEC,0x00A5E2FF,0x00BCF9FF,
    0x00001F00,0x00002A12,0x0000351E,0x0000422A,0x00004E36,0x000B5B44,0x00196A53,0x00287861,
    0x0033826B,0x0043927B,0x0054A38C,0x0065B49E,0x0077C6AF,0x008AD9C2,0x009EEDD6,0x00B5FFED,
    0x00002400,0x00003000,0x00003B00,0x00004700,0x0000530A,0x00106118,0x001E6F27,0x002D7E35,
    0x00378840,0x00479850,0x0059A961,0x006ABA72,0x007BCB84,0x008FDE97,0x00A3F2AB,0x00BAFFC2,
    0x00002300,0x00002F00,0x00003A00,0x00004600,0x00115200,0x001F6000,0x002E6E00,0x003C7C12,
    0x0047871C,0x0057972D,0x0068A83E,0x0079B94F,0x008ACA61,0x009EDD74,0x00B2F189,0x00C9FFA0,
    0x00001B00,0x00002700,0x000F3200,0x001C3E00,0x00284A00,0x00365800,0x00446600,0x00527500,
    0x005D7F00,0x006D8F19,0x007EA02B,0x008FB13D,0x00A0C24E,0x00B4D662,0x00C8EA76,0x00DFFF8D,
    0x00110E00,0x001D1A00,0x00292500,0x00353100,0x00413D00,0x004F4B00,0x005D5A00,0x006B6800,
    0x0076720B,0x0085821B,0x0097932D,0x00A8A43E,0x00B9B650,0x00CCC963,0x00E0DD77,0x00F7F48F,
};

// swizzed ntsc palette in RAM
const static DRAM_ATTR uint32_t atari_4_phase_ntsc[256] = {

    0x18181818,0x1A1A1A1A,0x1C1C1C1C,0x1F1F1F1F,0x21212121,0x24242424,0x27272727,0x2A2A2A2A,0x2D2D2D2D,0x30303030,0x34343434,0x38383838,0x3B3B3B3B,0x40404040,0x44444444,0x49494949,
    0x1A15210E,0x1C182410,0x1E1A2612,0x211D2915,0x231F2B18,0x26222E1A,0x2925311D,0x2C283420,0x2F2B3723,0x322E3A27,0x36323E2A,0x3A36412E,0x3D394532,0x423D4936,0x46424E3A,0x4B46523F,
    0x151A210E,0x171D2310,0x191F2613,0x1C222815,0x1E242B18,0x21272E1B,0x242A311D,0x272D3420,0x2A303724,0x2E333A27,0x31373D2A,0x353A412E,0x393E4532,0x3D424936,0x41474D3A,0x464B523F,
    0x101F1F10,0x13212113,0x15232315,0x18262618,0x1A28281A,0x1D2B2B1D,0x202E2E20,0x23313123,0x26343426,0x29383729,0x2D3B3B2D,0x303F3F31,0x34434234,0x38474738,0x3D4B4B3D,0x41505041,
    0x0E211A15,0x10231D17,0x13261F19,0x1528221C,0x182B241F,0x1B2E2721,0x1D312A24,0x20342D27,0x2337302A,0x273A332E,0x2A3E3731,0x2E413A35,0x32453E39,0x3649423D,0x3A4D4741,0x3F524B46,
    0x0E21151A,0x1024181C,0x12261A1E,0x15291D21,0x182B1F24,0x1A2E2226,0x1D312529,0x2034282C,0x23372B2F,0x273A2E33,0x2A3E3236,0x2E41353A,0x3245393E,0x36493D42,0x3A4E4246,0x3F52464B,
    0x101F111E,0x12211320,0x15241623,0x17261825,0x1A291B28,0x1D2C1E2B,0x202F202E,0x23322331,0x26352634,0x29382A37,0x2C3B2D3B,0x303F313E,0x34433542,0x38473946,0x3C4B3D4A,0x4150424F,
    0x141B0E21,0x161D1023,0x19201326,0x1B221528,0x1E25182B,0x21281B2E,0x242A1E30,0x272E2133,0x2A312436,0x2D34273A,0x30372B3D,0x343B2E41,0x383F3245,0x3C433649,0x40473A4D,0x454C3F52,
    0x19160E21,0x1B181024,0x1E1B1226,0x201D1529,0x2320172B,0x26231A2E,0x29261D31,0x2C292034,0x2F2C2337,0x322F273A,0x35322A3E,0x39362E41,0x3D3A3245,0x413E3649,0x45423A4E,0x4A473F52,
    0x1E11101F,0x20141222,0x22161424,0x25191727,0x271B1929,0x2A1E1C2C,0x2D211F2F,0x30242232,0x33272535,0x362A2838,0x3A2E2C3C,0x3E323040,0x41353343,0x46393847,0x4A3E3C4C,0x4F424150,
    0x210E131C,0x2311161E,0x25131820,0x28161B23,0x2A181D26,0x2D1B2028,0x301E232B,0x3321262E,0x36242931,0x3A272C35,0x3D2B3038,0x412E333C,0x45323740,0x49363B44,0x4D3B4048,0x523F444D,
    0x210E1817,0x24101B19,0x26121D1B,0x29151F1E,0x2B172221,0x2E1A2523,0x311D2826,0x34202B29,0x37232E2C,0x3A263130,0x3E2A3533,0x422E3837,0x45313C3B,0x4936403F,0x4E3A4543,0x523F4948,
    0x200F1D12,0x22111F14,0x25142217,0x27162419,0x2A19271C,0x2D1C2A1F,0x2F1F2C22,0x32222F25,0x35253328,0x3928362B,0x3C2C392F,0x402F3D32,0x44334136,0x4837453A,0x4C3B493E,0x51404E43,
    0x1C13200F,0x1F152311,0x21172513,0x241A2816,0x261D2A19,0x291F2D1B,0x2C22301E,0x2F253321,0x32283624,0x352C3928,0x392F3D2B,0x3C33402F,0x40374433,0x443B4837,0x493F4D3B,0x4D445140,
    0x1818220E,0x1A1A2410,0x1C1C2612,0x1F1F2915,0x21212B17,0x24242E1A,0x2727311D,0x2A2A3420,0x2D2D3723,0x30303A26,0x34343E2A,0x3838422E,0x3B3B4531,0x40404A36,0x44444E3A,0x4949533F,
    0x131C200F,0x151F2311,0x17212513,0x1A242816,0x1D262A19,0x1F292D1B,0x222C301E,0x252F3321,0x28323624,0x2C353928,0x2F393D2B,0x333C402F,0x37404433,0x3B444837,0x3F494D3B,0x444D5140,
};

const static DRAM_ATTR uint32_t atari_4_phase_ntsc_even[256] = {

    0x18180000,0x1A1A0000,0x1C1C0000,0x1F1F0000,0x21210000,0x24240000,0x27270000,0x2A2A0000,0x2D2D0000,0x30300000,0x34340000,0x38380000,0x3B3B0000,0x40400000,0x44440000,0x49490000,
    0x1A150000,0x1C180000,0x1E1A0000,0x211D0000,0x231F0000,0x26220000,0x29250000,0x2C280000,0x2F2B0000,0x322E0000,0x36320000,0x3A360000,0x3D390000,0x423D0000,0x46420000,0x4B460000,
    0x151A0000,0x171D0000,0x191F0000,0x1C220000,0x1E240000,0x21270000,0x242A0000,0x272D0000,0x2A300000,0x2E330000,0x31370000,0x353A0000,0x393E0000,0x3D420000,0x41470000,0x464B0000,
    0x101F0000,0x13210000,0x15230000,0x18260000,0x1A280000,0x1D2B0000,0x202E0000,0x23310000,0x26340000,0x29380000,0x2D3B0000,0x303F0000,0x34430000,0x38470000,0x3D4B0000,0x41500000,
    0x0E210000,0x10230000,0x13260000,0x15280000,0x182B0000,0x1B2E0000,0x1D310000,0x20340000,0x23370000,0x273A0000,0x2A3E0000,0x2E410000,0x32450000,0x36490000,0x3A4D0000,0x3F520000,
    0x0E210000,0x10240000,0x12260000,0x15290000,0x182B0000,0x1A2E0000,0x1D310000,0x20340000,0x23370000,0x273A0000,0x2A3E0000,0x2E410000,0x32450000,0x36490000,0x3A4E0000,0x3F520000,
    0x101F0000,0x12210000,0x15240000,0x17260000,0x1A290000,0x1D2C0000,0x202F0000,0x23320000,0x26350000,0x29380000,0x2C3B0000,0x303F0000,0x34430000,0x38470000,0x3C4B0000,0x41500000,
    0x141B0000,0x161D0000,0x19200000,0x1B220000,0x1E250000,0x21280000,0x242A0000,0x272E0000,0x2A310000,0x2D340000,0x30370000,0x343B0000,0x383F0000,0x3C430000,0x40470000,0x454C0000,
    0x19160000,0x1B180000,0x1E1B0000,0x201D0000,0x23200000,0x26230000,0x29260000,0x2C290000,0x2F2C0000,0x322F0000,0x35320000,0x39360000,0x3D3A0000,0x413E0000,0x45420000,0x4A470000,
    0x1E110000,0x20140000,0x22160000,0x25190000,0x271B0000,0x2A1E0000,0x2D210000,0x30240000,0x33270000,0x362A0000,0x3A2E0000,0x3E320000,0x41350000,0x46390000,0x4A3E0000,0x4F420000,
    0x210E0000,0x23110000,0x25130000,0x28160000,0x2A180000,0x2D1B0000,0x301E0000,0x33210000,0x36240000,0x3A270000,0x3D2B0000,0x412E0000,0x45320000,0x49360000,0x4D3B0000,0x523F0000,
    0x210E0000,0x24100000,0x26120000,0x29150000,0x2B170000,0x2E1A0000,0x311D0000,0x34200000,0x37230000,0x3A260000,0x3E2A0000,0x422E0000,0x45310000,0x49360000,0x4E3A0000,0x523F0000,
    0x200F0000,0x22110000,0x25140000,0x27160000,0x2A190000,0x2D1C0000,0x2F1F0000,0x32220000,0x35250000,0x39280000,0x3C2C0000,0x402F0000,0x44330000,0x48370000,0x4C3B0000,0x51400000,
    0x1C130000,0x1F150000,0x21170000,0x241A0000,0x261D0000,0x291F0000,0x2C220000,0x2F250000,0x32280000,0x352C0000,0x392F0000,0x3C330000,0x40370000,0x443B0000,0x493F0000,0x4D440000,
    0x18180000,0x1A1A0000,0x1C1C0000,0x1F1F0000,0x21210000,0x24240000,0x27270000,0x2A2A0000,0x2D2D0000,0x30300000,0x34340000,0x38380000,0x3B3B0000,0x40400000,0x44440000,0x49490000,
    0x131C0000,0x151F0000,0x17210000,0x1A240000,0x1D260000,0x1F290000,0x222C0000,0x252F0000,0x28320000,0x2C350000,0x2F390000,0x333C0000,0x37400000,0x3B440000,0x3F490000,0x444D0000,
};

const static DRAM_ATTR uint16_t atari_4_phase_ntsc_odd[256] = {

    0x1818,0x1A1A,0x1C1C,0x1F1F,0x2121,0x2424,0x2727,0x2A2A,0x2D2D,0x3030,0x3434,0x3838,0x3B3B,0x4040,0x4444,0x4949,
    0x210E,0x2410,0x2612,0x2915,0x2B18,0x2E1A,0x311D,0x3420,0x3723,0x3A27,0x3E2A,0x412E,0x4532,0x4936,0x4E3A,0x523F,
    0x210E,0x2310,0x2613,0x2815,0x2B18,0x2E1B,0x311D,0x3420,0x3724,0x3A27,0x3D2A,0x412E,0x4532,0x4936,0x4D3A,0x523F,
    0x1F10,0x2113,0x2315,0x2618,0x281A,0x2B1D,0x2E20,0x3123,0x3426,0x3729,0x3B2D,0x3F31,0x4234,0x4738,0x4B3D,0x5041,
    0x1A15,0x1D17,0x1F19,0x221C,0x241F,0x2721,0x2A24,0x2D27,0x302A,0x332E,0x3731,0x3A35,0x3E39,0x423D,0x4741,0x4B46,
    0x151A,0x181C,0x1A1E,0x1D21,0x1F24,0x2226,0x2529,0x282C,0x2B2F,0x2E33,0x3236,0x353A,0x393E,0x3D42,0x4246,0x464B,
    0x111E,0x1320,0x1623,0x1825,0x1B28,0x1E2B,0x202E,0x2331,0x2634,0x2A37,0x2D3B,0x313E,0x3542,0x3946,0x3D4A,0x424F,
    0x0E21,0x1023,0x1326,0x1528,0x182B,0x1B2E,0x1E30,0x2133,0x2436,0x273A,0x2B3D,0x2E41,0x3245,0x3649,0x3A4D,0x3F52,
    0x0E21,0x1024,0x1226,0x1529,0x172B,0x1A2E,0x1D31,0x2034,0x2337,0x273A,0x2A3E,0x2E41,0x3245,0x3649,0x3A4E,0x3F52,
    0x101F,0x1222,0x1424,0x1727,0x1929,0x1C2C,0x1F2F,0x2232,0x2535,0x2838,0x2C3C,0x3040,0x3343,0x3847,0x3C4C,0x4150,
    0x131C,0x161E,0x1820,0x1B23,0x1D26,0x2028,0x232B,0x262E,0x2931,0x2C35,0x3038,0x333C,0x3740,0x3B44,0x4048,0x444D,
    0x1817,0x1B19,0x1D1B,0x1F1E,0x2221,0x2523,0x2826,0x2B29,0x2E2C,0x3130,0x3533,0x3837,0x3C3B,0x403F,0x4543,0x4948,
    0x1D12,0x1F14,0x2217,0x2419,0x271C,0x2A1F,0x2C22,0x2F25,0x3328,0x362B,0x392F,0x3D32,0x4136,0x453A,0x493E,0x4E43,
    0x200F,0x2311,0x2513,0x2816,0x2A19,0x2D1B,0x301E,0x3321,0x3624,0x3928,0x3D2B,0x402F,0x4433,0x4837,0x4D3B,0x5140,
    0x220E,0x2410,0x2612,0x2915,0x2B17,0x2E1A,0x311D,0x3420,0x3723,0x3A26,0x3E2A,0x422E,0x4531,0x4A36,0x4E3A,0x533F,
    0x200F,0x2311,0x2513,0x2816,0x2A19,0x2D1B,0x301E,0x3321,0x3624,0x3928,0x3D2B,0x402F,0x4433,0x4837,0x4D3B,0x5140,
};

constexpr unsigned int NTSC_DEFAULT_WIDTH = 336;
constexpr unsigned int Screen_WIDTH = NTSC_DEFAULT_WIDTH;
constexpr unsigned int Screen_HEIGHT = 240;

uint8_t  ** _lines;
bool        bColorburstEnabled = true;

// Color clock frequency is 315/88 (3.57954545455)
// DAC_MHZ is 315/11 or 8x color clock
// 455/2 color clocks per line, round up to maintain phase
// HSYNCH period is 44/315*455 or 63.55555..us
// Field period is 262*44/315*455 or 16651.5555us

#define IRE(_x)          ((uint32_t)(((_x)+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)
#define NTSC_COLOR_CLOCKS_PER_SCANLINE 228       // really 227.5 for NTSC but want to avoid half phase fiddling for now
#define NTSC_LINES 262


static constexpr uint16_t NTSC_BURST_4[12] = {
    BLANKING_LEVEL + BLANKING_LEVEL / 2, BLANKING_LEVEL, BLANKING_LEVEL - BLANKING_LEVEL / 2, BLANKING_LEVEL,
    BLANKING_LEVEL + BLANKING_LEVEL / 2, BLANKING_LEVEL, BLANKING_LEVEL - BLANKING_LEVEL / 2, BLANKING_LEVEL,
    BLANKING_LEVEL + BLANKING_LEVEL / 2, BLANKING_LEVEL, BLANKING_LEVEL - BLANKING_LEVEL / 2, BLANKING_LEVEL,
};

volatile int _line_counter = 0;
volatile int _frame_counter = 0;

int _line_width;
int _samples_per_cc = 4; // 3 or 4

float _sample_rate;

int _hsync;
int _hsync_long;
int _burst_start;
int _burst_width;
int _active_start;

//====================================================================================================
//====================================================================================================
//
// low level HW setup of DAC/DMA/APLL/PWM
//

lldesc_t _dma_desc[4] = {0};
intr_handle_t _isr_handle;

extern "C"
void IRAM_ATTR video_isr(volatile void* buf);

// simple isr
static void IRAM_ATTR i2s_intr_handler_video(void *arg) {
    if (I2S0.int_st.out_eof)
        video_isr(((lldesc_t*)I2S0.out_eof_des_addr)->buf); // get the next line of video
    I2S0.int_clr.val = I2S0.int_st.val;                     // reset the interrupt
}

static esp_err_t start_dma(int line_width,int samples_per_cc, int ch = 1)
{
    periph_module_enable(PERIPH_I2S0_MODULE);

    // setup interrupt
    if (esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_INTRDISABLED,
        i2s_intr_handler_video, 0, &_isr_handle) != ESP_OK)
        return -1;

    // reset conf
    I2S0.conf.val = 1;
    I2S0.conf.val = 0;
    I2S0.conf.tx_right_first = 1;
    I2S0.conf.tx_mono = (ch == 2 ? 0 : 1);

    I2S0.conf2.lcd_en = 1;
    I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S0.sample_rate_conf.tx_bits_mod = 16;
    I2S0.conf_chan.tx_chan_mod = (ch == 2) ? 0 : 1;

    // Create TX DMA buffers
    for (int i = 0; i < 2; i++) {
        int n = line_width*2*ch;
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

    switch (samples_per_cc) {
        case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // 10.7386363636 3x NTSC (10.7386398315mhz)
        case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // 14.3181818182 4x NTSC (14.3181864421mhz)
    }
    I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider’s integral value.
    I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider’s numerator value.
    I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider’s denominator value
    I2S0.sample_rate_conf.tx_bck_div_num = 1;
    I2S0.clkm_conf.clka_en = 1;                 // Set this bit to enable clk_apll.
    I2S0.fifo_conf.tx_fifo_mod = (ch == 2) ? 0 : 1; // 32-bit dual or 16-bit single channel data

    dac_output_enable(DAC_CHANNEL_1);           // DAC, video on GPIO25
    dac_i2s_enable();                           // start DAC!

    I2S0.conf.tx_start = 1;                     // start DMA!
    I2S0.int_clr.val = 0xFFFFFFFF;
    I2S0.int_ena.out_eof = 1;
    I2S0.out_link.start = 1;
    return esp_intr_enable(_isr_handle);        // start interruprs!
}

static void stop_dma()
{
    dac_i2s_disable();
    dac_output_disable(DAC_CHANNEL_1);
    rtc_clk_apll_enable(0,0x00,0x00,0x00,0);
    // Dispose TX DMA buffers
    for (int i = 0; i < 2; i++) {
        free((void *)(_dma_desc[i].buf));
    }
    esp_intr_free(_isr_handle);
    periph_module_disable(PERIPH_I2S0_MODULE);
}

void video_init_hw(int line_width, int samples_per_cc)
{
    // setup apll 4x NTSC colorburst rate
    start_dma(line_width,samples_per_cc,1);
}

//====================================================================================================


uint32_t cpu_ticks()
{
  return xthal_get_ccount();
}

uint32_t us() {
    return cpu_ticks()/240;
}

static int usec(float us)
{
    uint32_t r = (uint32_t)(us*_sample_rate);
    return ((r + _samples_per_cc)/(_samples_per_cc << 1))*(_samples_per_cc << 1);  // multiple of color clock, word align
}


//===================================================================================================
//===================================================================================================
//===================================================================================================
//===================================================================================================
// ntsc tables
// AA AA                // 2 pixels, 1 color clock - atari
// AA AB BB             // 3 pixels, 2 color clocks - nes
// AAA ABB BBC CCC      // 4 pixels, 3 color clocks - sms

// cc == 3 gives 684 samples per line, 3 samples per cc, 3 pixels for 2 cc
// cc == 4 gives 912 samples per line, 4 samples per cc, 2 pixels per cc

    void video_init()
    {
        _sample_rate = 315.0/88 * _samples_per_cc;   // DAC rate
        _line_width = NTSC_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;
        _hsync_long = usec(63.555-4.7);
        _active_start = usec(_samples_per_cc == 4 ? 10 : 10.5);
        _hsync = usec(4.7);

        video_init_hw(_line_width,_samples_per_cc);    // init the hardware
    }

    /// @brief So-called "hi-res" blitter, handling modes with 640 pixels per line
    /// @param src pointer to a line in the framebuffer
    /// @param dst DMA buffer
    /// @return 
    void IRAM_ATTR blitter_0(uint8_t *src, uint16_t *dst)
    {
      static const uint32_t STEP = 4;
      static const uint32_t MASK_EVEN = 0x0000FFFF;
      static const uint32_t MASK_ODD = 0xFFFF0000;

      uint32_t *d = (uint32_t *)(dst + 32);
      for (int i = 0; i < RawCompositeVideoBlitter::NTSC_DEFAULT_WIDTH; i += STEP) // 84 steps, 4 pixels per step
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
    void IRAM_ATTR blitter_1(uint8_t *src, uint16_t *dst)
    {
      static const uint32_t STEP = 4;

      uint32_t *d = (uint32_t *)(dst + 32);
      for (int i = 0; i < RawCompositeVideoBlitter::NTSC_DEFAULT_WIDTH; i += STEP) // 84 steps, 4 pixels per step
      {
        d[0] = atari_4_phase_ntsc[src[0]] << 0;
        d[1] = atari_4_phase_ntsc[src[1]] << 8;
        d[2] = atari_4_phase_ntsc[src[2]] << 0;
        d[3] = atari_4_phase_ntsc[src[3]] << 8;
        d += STEP;
        src += STEP;
      }
    }

    /// @brief Experimental blitter, actually not in use.
    /// @param src pointer to a line in the framebuffer
    /// @param dst DMA buffer
    /// @return 
    void IRAM_ATTR blitter_2(uint8_t *src, uint16_t *dst)
    {
      // static uint32_t line[RawCompositeVideoBlitter::NTSC_DEFAULT_WIDTH];
      static const uint32_t STEP = 4;

      uint32_t *d = (uint32_t *)(dst + 32);
      for (int i = 0; i < RawCompositeVideoBlitter::NTSC_DEFAULT_WIDTH; i += STEP) // 84 steps, 4 pixels per step
      {
        d[0] = atari_4_phase_ntsc[src[0]];
        d[1] = atari_4_phase_ntsc[src[1]] << 8;
        d[2] = atari_4_phase_ntsc[src[2]];
        d[3] = atari_4_phase_ntsc[src[3]] << 8;
        d += STEP;
        src += STEP;
      }
    }

void IRAM_ATTR burst(uint16_t* line)
{
    int i,phase;
    switch (_samples_per_cc) {
        case 4:
            // 4 samples per color clock
            for (i = _hsync; i < _hsync + (4*10); i += 4) {
                if (bColorburstEnabled)
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

void __attribute__((optimize("-Ofast"))) IRAM_ATTR sync(uint16_t *line, int syncwidth) {
  for (int i = 0; i < syncwidth; i++)
    line[i] = SYNC_LEVEL;
}

void __attribute__((optimize("-Ofast"))) IRAM_ATTR blank(uint16_t *line, int syncwidth) {
  for (int i = syncwidth; i < _line_width; i++)
    line[i] = BLANKING_LEVEL;
}

void IRAM_ATTR blanking(uint16_t* line, bool vbl)
{
    int syncwidth = vbl ? _hsync_long : _hsync;
    sync(line,syncwidth);
    for (int i = syncwidth; i < _line_width; i++)
        line[i] = BLANKING_LEVEL;
    if (!vbl)
        burst(line);    // no burst during vbl
}

// Wait for blanking before starting drawing
// avoids tearing in our unsynchonized world
#ifdef ESP_PLATFORM
    void video_sync()
    {
      if (!_lines)
        return;
      int n = 0;
      if (_line_counter < Screen_HEIGHT)
        n = (Screen_HEIGHT - _line_counter)*1000/15720;
      vTaskDelay(n+1);
    }
#endif

// Workhorse ISR handles audio and video updates
extern "C"
void IRAM_ATTR video_isr(volatile void* vbuf)
{
    if (!_lines)
        return;

    int i = _line_counter++;
    uint16_t* buf = (uint16_t*)vbuf;
    // ntsc
    if (i < Screen_HEIGHT) {                // active video
      sync(buf,_hsync);
      burst(buf);
      _blitter(_lines[i],buf + _active_start);
    } else  {
      if (i < (Screen_HEIGHT + 5)) {   // post render/black
        sync(buf, _hsync);
        blank(buf, _hsync);
        burst(buf);
      } else if (i < (Screen_HEIGHT + 8)) {   // vsync
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
} // Namespace RawCompositeVideoBlitter
