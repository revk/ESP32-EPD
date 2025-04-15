#include "revk.h"
#include "gfx.h"
#include "EPD.h"

#define	UNUSED __attribute__((unused))
static const char UNUSED TAG[] = "CLOCK";

void
widget_clock (uint16_t s, const char *c)
{
   time_t now = time (0);
   if (now < 1000000000)
      return;
   struct tm t;
   localtime_r (&now, &t);
   uint8_t surround = 0,
      ticks = 0,
      numbers = 0;
   if (s & 0x8000)
      surround = 1;
   if (s & 0x4000)
      ticks = 1;
   if (s & 0x2000)
      numbers = 1;
   s &= 0xFFF;
   if (!s)
      s = 20;
   gfx_pos_t ox,
     oy;
   uint16_t a;
   gfx_draw (s, s, 0, 0, &ox, &oy);
   gfx_pos_t nx = gfx_x (),
      ny = gfx_y ();
   gfx_align_t na = gfx_a ();
   // half pixels
   gfx_pos_t cx = ox * 2 + s,
      cy = oy * 2 + s;
   if (surround)
      gfx_circle2 (cx, cy, s - s / 50, s / 50);
   if (numbers)
      for (int h = 1; h <= 12; h++)
      {
         a = h * 360 / 12;
         char temp[3];
         sprintf (temp, "%d", h);
         gfx_pos ((cx + isin (a, s * 17 / 20)) / 2, (cy - icos (a, s * 17 / 20)) / 2, GFX_C | GFX_M);
         gfx_text (0, s / 180 ? : 1, temp);
      }
   if (ticks)
      for (a = 0; a < 360; a += 30)
         gfx_line2 (cx + isin (a, s * 9 / 10), cy - icos (a, s * 9 / 10), cx + isin (a, s), cy - icos (a, s), s / 40);
   ESP_LOGE (TAG, "cx %d cy %d", cx, cy);
   // Hands
#ifndef	GFX_EPD
   a = t.tm_sec * 360 / 60;
   gfx_line2 (cx, cy, cx + isin (a, s), cy - icos (a, s), s / 40);
#endif
   a = t.tm_min * 360 / 60;
   gfx_line2 (cx, cy, cx + isin (a, s * 7 / 10), cy - icos (a, s * 7 / 10), s / 30);
   a = t.tm_hour * 360 / 12;
   gfx_line2 (cx, cy, cx + isin (a, s * 5 / 10), cy - icos (a, s * 5 / 10), s / 15);
   gfx_pos (nx, ny, na);
}
