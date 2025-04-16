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
   uint32_t a;
   gfx_draw (s, s, 0, 0, &ox, &oy);
   gfx_pos_t nx = gfx_x (),
      ny = gfx_y ();
   gfx_align_t na = gfx_a ();
   // half pixels
   gfx_pos_t cx = ox * 2 + s,
      cy = oy * 2 + s;
   if (surround)
   {
      gfx_circle2 (cx, cy, s - s / 50, s / 50 ? : 1);
      s = s * 49 / 50;
   }
   uint8_t o = 19;
   if (ticks)
      o -= 2;
   if (numbers)
      for (int h = 1; h <= 12; h++)
      {
         a = h * 360 / 12;
         char temp[3];
         sprintf (temp, "%d", h);
         gfx_pos ((cx + isin (a, s * o / 20)) / 2, (cy - icos (a, s * o / 20)) / 2, GFX_C | GFX_M);
         gfx_text (0, s / 180 ? : 1, temp);
      }
   if (ticks)
      for (a = 0; a < 360; a += 30)
         gfx_line2 (cx + isin (a, s * 19 / 20), cy - icos (a, s * 19 / 20), cx + isin (a, s - s / 50), cy - icos (a, s - s / 50),
                    s / 50 ? : 1);
   // Hands
   uint32_t sec = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
#ifndef	GFX_EPD
   a = sec * 6;
   gfx_line2 (cx, cy, cx + isin (a, s), cy - icos (a, s), s / 40 ? : 1);
#endif
   a = (sec + 5) / 10;
   gfx_line2 (cx, cy, cx + isin (a, s * 7 / 10), cy - icos (a, s * 7 / 10), s / 30 ? : 1);
   a = (sec + 300) / 600;
   gfx_line2 (cx, cy, cx + isin (a, s * 5 / 10), cy - icos (a, s * 5 / 10), s / 15 ? : 1);
   gfx_pos (nx, ny, na);
}
