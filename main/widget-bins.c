#include "revk.h"
#include "gfx.h"
#include "EPD.h"

static const char TAG[] = "BINS";

typedef struct icon_s
{
   struct icon_s *next;
   file_t *file;
   char *name;
   char *colour;
   gfx_pos_t w,
     h;
   gfx_pos_t start,
     end;
} icon_t;

void
widget_bins (int8_t s, const char *c)
{
   time_t now = time (0);
   if (!c || !*c || now < 1000000000)
      return;
   if (!s)
      s = 5;                    // Text size for day
   int s2 = -s / 2;             // Text size for icons
   gfx_align_t a = gfx_a ();
   file_t *bins = download ((char *) c);
   if (!bins || !bins->size || !bins->data)
      return;
   jo_t j = jo_parse_mem (bins->data, bins->size);
   if (!j)
      return;
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
      display = collect - 5 * 86400;
   if (!leds && collect)
      leds = collect - 12 * 3600;
   if (!clear && collect)
      clear = collect * 12 * 3600;
   if (!cache)
      cache = clear;
   if (!cache)
      cache = time (0) + 3600;
   bins->cache = cache;
   if (display && clear && display < now && now < clear)
   {
      // Icons
      icon_t *icons = NULL,
         **ie = &icons;
      char *base = NULL;
      if (jo_find (j, "baseurl") == JO_STRING)
         base = jo_strdup (j);
      if (jo_find (j, "bins") == JO_ARRAY)
      {
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
                     i->name = jo_strdup (j);
                  else if (!strcmp (tag, "colour"))
                     i->colour = jo_strdup (j);
                  else if (!strcmp (tag, "icon"))
                  {
                     char *leaf = jo_strdup (j);
                     if (leaf)
                     {
                        char *fn;
                        asprintf (&fn, "%s/%s", base ? : "", leaf);
                        free (leaf);
                        i->file = download (fn);
                        free (fn);
                        if (i->file && (!i->file->data || !i->file->size || !i->file->w))
                           i->file = NULL;
                     }
                  }
               }
            }
         }
      }
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
               gfx_text_size (s2, i->name, &i->w, &i->h);
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
         gfx_text_size (s, day = longday[tm.tm_wday], &dayw, &dayh);
         if (dayw > space)
            gfx_text_size (s, day = shortday[tm.tm_wday], &dayw, &dayh);
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
               {                // End of line
                  sol->start = w;
                  i->end = lh;
                  if (w > width)
                     width = w;
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
            gfx_text (s, day);
            oy += dayh;
         }
         if ((a & GFX_M) != GFX_B)
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
               ESP_LOGE (TAG, "%s ox=%d x=%d y=%d", i->name, ox, x - ox, oy);
               // Maybe T/B align in row?
               if (i->file)
                  plot (i->file, x, oy);        // Icon
               else
               {                // Name
                  gfx_pos (x, oy, GFX_L | GFX_T);
                  gfx_text (s2, i->name);
               }
               x += i->w;
               if (i->end)
               {
                  x = ox;
                  oy += i->end;
               }
            }
         }
         if ((a & GFX_M) == GFX_B)
            showday ();
      }
      // Cleanup
      free (base);
      while (icons)
      {
         ESP_LOGD (TAG, "Icon name=%s colour=%s size=%ld width=%u height=%u start=%u end=%u", icons->name ? : "-",
                   icons->colour ? : "-", icons->file ? icons->file->size : 0, icons->w, icons->h, icons->start, icons->end);
         icon_t *next = icons->next;
         free (icons->name);
         free (icons->colour);
         free (icons);
         icons = next;
      }
   }
   jo_free (&j);
}
