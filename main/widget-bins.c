#include "revk.h"
#include "gfx.h"
#include "EPD.h"

#define	UNUSED __attribute__((unused))
static const char UNUSED TAG[] = "BINS";

typedef struct icon_s
{
   struct icon_s *next;
   file_t *file;
   char *name;
   gfx_pos_t w,
     h,
     dy;
   gfx_pos_t start,
     end;
} icon_t;

void
widget_bins (uint16_t s, const char *c)
{
#ifndef	CONFIG_GFX_BUILD_SUFFIX_GFXNONE
   time_t now = time (0);
   if (!c || !*c || now < 1000000000)
      return;
   struct tm today;
   localtime_r (&now, &today);
   uint8_t flags = 0,
      bottom = 0;
   if (s & 0x8000)
      bottom = 1;
   if (s & 0x4000)
      flags |= GFX_TEXT_LIGHT;
   s &= 0xFFF;
   if (!s)
      s = 9;                    // Text size for day
   uint8_t s1 = s;
   uint8_t s2 = s1 / 2;         // Text size for icons
   if (!s2)
      s2 = 1;
   gfx_align_t a = gfx_a ();
   file_t *bins = download ((char *) c, ".json", 0,cachebins);
   char *jbins = NULL;
   uint32_t size = 0;
   xSemaphoreTake (file_mutex, portMAX_DELAY);
   if (bins && bins->data && (size = bins->size))
   {
      jbins = mallocspi (size);
      if (jbins)
         memcpy (jbins, bins->data, size);
   }
   xSemaphoreGive (file_mutex);
   if (jbins)
   {
      jo_t j = jo_parse_mem (jbins, size);
      if (j)
      {
         // Times
         time_t cache = 0,
            display = 0,
            leds = 0,
            collect = 0,
            clear = 0;
         if (jo_find (j, "cache") == JO_STRING)
            cache = jo_read_datetime (j);
         if (jo_find (j, "display") == JO_STRING)
            display = jo_read_datetime (j);
         if (jo_find (j, "leds") == JO_STRING)
            leds = jo_read_datetime (j);
         if (jo_find (j, "collect") == JO_STRING)
            collect = jo_read_datetime (j);
         if (jo_find (j, "clear") == JO_STRING)
            clear = jo_read_datetime (j);
         if (!display && collect)
            display = collect - 7 * 86400;
         if (!leds && collect)
            leds = collect - 12 * 3600;
         if (!clear && collect)
            clear = collect * 12 * 3600;
         if (!cache)
            cache = clear;
	 if(cache>now)
         bins->cached = uptime()+cache-now;
         char *led = NULL;
         if (display && clear && display < now && now < clear)
         {
            size_t ledl;
            FILE *ledf = open_memstream (&led, &ledl);
            // Icons
            icon_t *icons = NULL,
               **ie = &icons;
            char *base = NULL;
            if (jo_find (j, "baseurl") == JO_STRING)
               base = jo_strdup (j);
            if (jo_find (j, "bins") == JO_ARRAY)
            {
               char hasdesc = 0;
               while (jo_next (j) == JO_OBJECT)
               {
                  icon_t *i = NULL;
                  while (jo_next (j) == JO_TAG)
                  {
                     if (!i)
                     {
                        i = mallocspi (sizeof (*i));
                        if (!i)
                           break;
                        memset (i, 0, sizeof (*i));
                        *ie = i;
                        ie = &i->next;
                     }
                     char tag[10];
                     jo_strncpy (j, tag, sizeof (tag));
                     if (jo_next (j) == JO_STRING)
                     {
                        if (!strcmp (tag, "name"))
                        {
                           i->name = jo_strdup (j);
                           if (!hasdesc && i->name && gfx_text_desc (i->name))
                              hasdesc = 1;
                        } else if (!strcmp (tag, "colour"))
                        {
                           char *c = jo_strdup (j);
                           if (c && *c)
                              fwrite (c, strlen (c), 1, ledf);
                           free (c);
                        } else if (!strcmp (tag, "icon"))
                        {
                           char *leaf = jo_strdup (j);
                           if (leaf)
                           {
                              char *fn;
                              asprintf (&fn, "%s/%s", base ? : "", leaf);
                              free (leaf);
                              i->file = download (fn, ".png", 0,cachepng);
                              free (fn);
                              if (i->file && (!i->file->data || !i->file->size || !i->file->w))
                                 i->file = NULL;
                           }
                        }
                     }
                  }
               }
               if (!hasdesc)
                  flags |= GFX_TEXT_DESCENDERS;
            }
            fclose (ledf);
            if (icons)
            {
               for (icon_t * i = icons; i; i = i->next)
               {
                  if (!i->name)
                     i->name = strdup ("?");
                  if (i->file)
                  {
                     i->w = i->file->w;
                     i->h = i->file->h;
                  } else
                     gfx_text_size (flags, s2, i->name, &i->w, &i->h);
               }
               // Position
               gfx_pos_t space = 0;
               {
                  gfx_pos_t l = gfx_x ();
                  gfx_pos_t r = gfx_width () - l;
                  if ((a & GFX_C) == GFX_L)
                     space = r;
                  else if ((a & GFX_C) == GFX_R)
                     space = l;
                  else
                     space = (l < r ? l : r) * 2;

               }
               struct tm tm;
               localtime_r (&collect, &tm);
               gfx_pos_t dayw,
                 dayh;
               const char *day;
               gfx_text_size (flags & ~GFX_TEXT_DESCENDERS, s1, day =
                              ((today.tm_yday ==
                                tm.tm_yday) ? "*TODAY*" : (today.tm_yday + 1 == tm.tm_yday) ? "TOMORROW" : longday[tm.tm_wday]),
                              &dayw, &dayh);
               if (dayw > space)
                  gfx_text_size (flags & ~GFX_TEXT_DESCENDERS, s1, day = shortday[tm.tm_wday], &dayw, &dayh);
               if (dayw > space)
                  space = dayw;
               gfx_pos_t width = dayw,
                  height = dayh;
               {
                  gfx_pos_t w = 0,
                     lh = 0;
                  icon_t *sol = NULL;
                  for (icon_t * i = icons; i; i = i->next)
                  {
                     if (!w)
                        sol = i;
                     w += i->w;
                     if (i->h > lh)
                        lh = i->h;
                     if (!i->next || w + i->next->w > space)
                     {          // End of line
                        sol->start = w;
                        i->end = lh;
                        if (w > width)
                           width = w;
                        if ((a & GFX_M) != GFX_T)
                           while (sol)
                           {
                              if ((a & GFX_M) == GFX_B)
                                 sol->dy = lh - sol->h;
                              else
                                 sol->dy = (lh - sol->h) / 2;
                              if (sol == i)
                                 break;
                              sol = sol->next;
                           }
                        height += lh;
                        lh = 0;
                        w = 0;
                     }
                  }
               }
               ESP_LOGD (TAG, "Space %d Width %d Height %d", space, width, height);
               // Plot
               gfx_pos_t ox,
                 oy;
               gfx_draw (width, height, 0, 0, &ox, &oy);
               void showday (void)
               {
                  if ((a & GFX_C) == GFX_L)
                     gfx_pos (ox, oy, GFX_L | GFX_T);
                  else if ((a & GFX_C) == GFX_R)
                     gfx_pos (ox + width, oy, GFX_R | GFX_T);
                  else
                     gfx_pos (ox + width / 2, oy, GFX_C | GFX_T);
                  gfx_text (flags & ~GFX_TEXT_DESCENDERS, s1, day);
                  oy += dayh;
               }
               if (!bottom)
                  showday ();
               {
                  gfx_pos_t x = ox;
                  for (icon_t * i = icons; i; i = i->next)
                  {
                     if (i->start)
                     {
                        if ((a & GFX_C) == GFX_R)
                           x += width - i->start;
                        else if ((a & GFX_C) == GFX_C)
                           x += (width - i->start) / 2;
                     }
                     if (i->file)
                        plot (i->file, x, oy + i->dy,
#ifdef	GFX_EPD
                              gfxinvert
#else
                              0
#endif
                           );   // Icon
                     else
                     {          // Name
                        gfx_pos (x, oy + i->dy, GFX_L | GFX_T);
                        gfx_text (flags, s2, i->name);
                     }
                     x += i->w;
                     if (i->end)
                     {
                        x = ox;
                        oy += i->end;
                     }
                  }
               }
               if (bottom)
                  showday ();
               setlights (leds < now ? led : NULL);
            }
            // Cleanup
            while (icons)
            {
               ESP_LOGD (TAG, "Icon name=%s size=%ld width=%d height=%d start=%d end=%d dy=%d led=%s", icons->name ? : "-",
                         icons->file ? icons->file->size : 0, icons->w, icons->h, icons->start, icons->end, icons->dy, led ? : "-");
               icon_t *next = icons->next;
               free (icons->name);
               free (icons);
               icons = next;
            }
            free (base);
            free (led);
         } else
            setlights (NULL);
         jo_free (&j);
      }
      free (jbins);
   }
#endif
}
