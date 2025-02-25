/* EPDapp */
/* Copyright ©2019 - 2025 Adrian Kennard, Andrews & Arnold Ltd.See LICENCE file for details .GPL 3.0 */

static const char TAG[] = "EPD";

#include "revk.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "esp_vfs_fat.h"
#include <driver/sdmmc_host.h>
#include "gfx.h"
#include "iec18004.h"
#include <hal/spi_types.h>
#include <driver/gpio.h>
#include <lwpng.h>

#define	LEFT	0x80            // Flags on font size
#define	RIGHT	0x40
#define	LINE	0x20
#define	MASK	0x1F
#define MINSIZE	4

const char sd_mount[] = "/sd";
uint64_t sdsize = 0,            // SD card data
   sdfree = 0;

static struct
{                               // Flags
   uint8_t wificonnect:1;
   uint8_t redraw:1;
   uint8_t lightoverride:1;
   uint8_t startup:1;
} volatile b = { 0 };

volatile uint32_t override = 0;

led_strip_handle_t strip = NULL;
sdmmc_card_t *card = NULL;

static void *
my_alloc (void *opaque, uInt items, uInt size)
{
   return mallocspi (items * size);
}

static void
my_free (void *opaque, void *address)
{
   free (address);
}

const char *
gfx_qr (const char *value, int max)
{
#ifndef	CONFIG_GFX_NONE
   unsigned int width = 0;
 uint8_t *qr = qr_encode (strlen (value), value, widthp:&width);
   if (!qr)
      return "Failed to encode";
   if (max < width)
      return "No space";
   if (max > gfx_width () || max > gfx_height ())
      return "Too big";
   int s = max / width;
   gfx_pos_t ox,
     oy;
   gfx_draw (max, max, 0, 0, &ox, &oy);
   int d = (max - width * s) / 2;
   ox += d;
   oy += d;
   for (int y = 0; y < width; y++)
      for (int x = 0; x < width; x++)
         if (qr[width * y + x] & QR_TAG_BLACK)
            for (int dy = 0; dy < s; dy++)
               for (int dx = 0; dx < s; dx++)
                  gfx_pixel (ox + x * s + dx, oy + y * s + dy, 0xFF);
   free (qr);
#endif
   return NULL;
}

const char *const longday[] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };

time_t
parse_time (const char *t)
{
   struct tm tm = { 0 };
   int y = 0,
      m = 0,
      d = 0,
      H = 0,
      M = 0,
      S = 0;
   sscanf (t, "%d-%d-%d %d:%d:%d", &y, &m, &d, &H, &M, &S);
   tm.tm_year = y - 1900;
   tm.tm_mon = m - 1;
   tm.tm_mday = d;
   tm.tm_hour = H;
   tm.tm_min = M;
   tm.tm_sec = S;
   tm.tm_isdst = -1;
   return mktime (&tm);
}

void
showlights (const char *rgb)
{
   if (!strip)
      return;
   const char *c = rgb;
   for (int i = 0; i < leds; i++)
   {
      revk_led (strip, i, 255, revk_rgb (*c));
      if (*c)
         c++;
      if (!*c)
         c = rgb;
   }
   led_strip_refresh (strip);
}

const char *
app_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{
   char value[1000];
   int len = 0;
   *value = 0;
   if (j)
   {
      len = jo_strncpy (j, value, sizeof (value));
      if (len < 0)
         return "Expecting JSON string";
      if (len > sizeof (value))
         return "Too long";
   }
   if (client || !prefix || target || strcmp (prefix, topiccommand) || !suffix)
      return NULL;
   // Not for us or not a command from main MQTT
   if (!strcmp (suffix, "setting"))
   {
      b.redraw = 1;
      return "";
   }
   if (!strcmp (suffix, "connect"))
   {
      lwmqtt_subscribe (revk_mqtt (0), "DEFCON/#");
      return "";
   }
   if (!strcmp (suffix, "shutdown"))
   {
      if (card)
      {
         esp_vfs_fat_sdcard_unmount (sd_mount, card);
         card = NULL;
      }
      return "";
   }
   if (!strcmp (suffix, "wifi") || !strcmp (suffix, "ipv6") || !strcmp (suffix, "ap"))
   {
      b.wificonnect = 1;
      return "";
   }
   if (strip && !strcmp (suffix, "rgb"))
   {
      b.lightoverride = (*value ? 1 : 0);
      showlights (value);
      return "";
   }
   return NULL;
}

typedef struct image_s
{
   struct image_s *next;
   char *url;
   time_t changed;
   uint32_t size;
   uint32_t w;
   uint32_t h;
   uint8_t *data;
} image_t;

image_t *images = NULL;

image_t *
download (const char *url)
{
   image_t *i;
   for (i = images; i && strcmp (i->url, url); i = i->next);
   if (!i)
   {
      i = malloc (sizeof (*i));
      memset (i, 0, sizeof (*i));
      i->url = strdup (url);
      i->next = images;
      images = i;
   }
   ESP_LOGD (TAG, "Get %s", i->url);
   time_t now = time (0);
   int32_t len = 0;
   uint8_t *buf = NULL;
   esp_http_client_config_t config = {
      .url = i->url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 20000,
   };
   int response = -1;
   if (!revk_link_down ())
   {
      esp_http_client_handle_t client = esp_http_client_init (&config);
      if (client)
      {
         if (i->changed)
         {
            char when[50];
            struct tm t;
            gmtime_r (&i->changed, &t);
            strftime (when, sizeof (when), "%a, %d %b %Y %T GMT", &t);
            esp_http_client_set_header (client, "If-Modified-Since", when);
         }
         if (!esp_http_client_open (client, 0))
         {
            len = esp_http_client_fetch_headers (client);
            buf = mallocspi (len);
            if (buf)
               len = esp_http_client_read_response (client, (char *) buf, len);
            response = esp_http_client_get_status_code (client);
            if (response != 200 && response != 304)
               ESP_LOGE (TAG, "Bad response %s (%d)", i->url, response);
            esp_http_client_close (client);
         }
         esp_http_client_cleanup (client);
      }
   }
   ESP_LOGD (TAG, "Got %s %d", i->url, response);
   if (response != 304)
   {
      if (response != 200)
      {
         jo_t j = jo_object_alloc ();
         jo_string (j, "url", i->url);
         if (response && response != -1)
            jo_int (j, "response", response);
         if (len == -ESP_ERR_HTTP_EAGAIN)
            jo_string (j, "error", "timeout");
         else if (len)
            jo_int (j, "len", len);
         revk_error ("image", &j);
      }
      if (buf)
      {
         if (i->data && i->size == len && !memcmp (buf, i->data, len))
         {
            free (buf);
            response = 0;       // No change
         } else
         {                      // Change
            free (i->data);
            i->data = buf;
            i->size = len;
            lwpng_get_info (i->size, i->data, &i->w, &i->h);
            i->changed = now;
            ESP_LOGE (TAG, "Image %s len %lu width %lu height %lu", i->url, i->size, i->w, i->h);
         }
         buf = NULL;
      }
   }
   if (card)
   {                            // SD
      char *s = strrchr (i->url, '/');
      if (s)
      {
         char *fn = NULL;
         asprintf (&fn, "%s%s", sd_mount, s);
         if (i->data && response == 200)
         {                      // Save to card
            FILE *f = fopen (fn, "w");
            if (f)
            {
               jo_t j = jo_object_alloc ();
               if (fwrite (i->data, i->size, 1, f) != 1)
                  jo_string (j, "error", "write failed");
               fclose (f);
               jo_string (j, "write", fn);
               revk_info ("SD", &j);
               ESP_LOGE (TAG, "Write %s", fn);
            } else
               ESP_LOGE (TAG, "Write fail %s", fn);
         } else if (!i->data || (response && response != 304))
         {                      // Load from card
            FILE *f = fopen (fn, "r");
            if (f)
            {
               struct stat s;
               fstat (fileno (f), &s);
               free (buf);
               buf = mallocspi (s.st_size);
               if (buf)
               {
                  if (fread (buf, s.st_size, 1, f) == 1)
                  {
                     if (i->data && i->size == s.st_size && !memcmp (buf, i->data, i->size))
                     {
                        free (buf);
                        response = 0;   // No change
                     } else
                     {
                        ESP_LOGE (TAG, "Read %s", fn);
                        jo_t j = jo_object_alloc ();
                        jo_string (j, "read", fn);
                        revk_info ("SD", &j);
                        response = 200; // Treat as received
                        free (i->data);
                        i->data = buf;
                        i->size = s.st_size;
                        lwpng_get_info (i->size, i->data, &i->w, &i->h);
                     }
                     buf = NULL;
                  }
               }
               fclose (f);
            } else
               ESP_LOGE (TAG, "Read fail %s", fn);
         }
         free (fn);
      }
   }
   free (buf);
   if (i->data && !i->w)
   {                            // Not PNG
      free (i->data);
      i->data = NULL;
   }
   return i;
}

typedef struct plot_s
{
   gfx_pos_t ox,
     oy;
} plot_t;

const char *
plot (void *opaque, uint32_t x, uint32_t y, uint16_t r, uint16_t g, uint16_t b, uint16_t a)
{
   plot_t *p = opaque;
   if (a & 0x8000)
      gfx_pixel (p->ox + x, p->oy + y, (g & 0x8000) ? 255 : 0);
   return NULL;
}

#ifdef	CONFIG_REVK_APCONFIG
#error 	Clash with CONFIG_REVK_APCONFIG set
#endif

void
app_main ()
{
   revk_boot (&app_callback);
   revk_start ();

   if (leds && rgb.set)
   {
      led_strip_config_t strip_config = {
         .strip_gpio_num = (rgb.num),
         .max_leds = leds,
         .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
         .led_model = LED_MODEL_WS2812, // LED strip model
         .flags.invert_out = rgb.invert,        // whether to invert the output signal(useful when your hardware has a level inverter)
      };
      led_strip_rmt_config_t rmt_config = {
         .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
         .resolution_hz = 10 * 1000 * 1000,     // 10 MHz
         .flags.with_dma = true,
      };
      REVK_ERR_CHECK (led_strip_new_rmt_device (&strip_config, &rmt_config, &strip));
      showlights ("b");
   }
   if (gfxena.set)
   {
      gpio_reset_pin (gfxena.num);
      gpio_set_direction (gfxena.num, GPIO_MODE_OUTPUT);
      gpio_set_level (gfxena.num, gfxena.invert);       // Enable
   }
   {
    const char *e = gfx_init (cs: gfxcs.num, sck: gfxsck.num, mosi: gfxmosi.num, dc: gfxdc.num, rst: gfxrst.num, busy: gfxbusy.num, flip: gfxflip, direct: 1, invert:gfxinvert);
      if (e)
      {
         ESP_LOGE (TAG, "gfx %s", e);
         jo_t j = jo_object_alloc ();
         jo_string (j, "error", "Failed to start");
         jo_string (j, "description", e);
         revk_error ("gfx", &j);
      }
   }
   if (sdcmd.set)
   {
      revk_gpio_input (sdcd);
      sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT ();
      slot.clk = sdclk.num;
      slot.cmd = sdcmd.num;
      slot.d0 = sddat0.num;
      slot.d1 = sddat1.set ? sddat1.num : -1;
      slot.d2 = sddat2.set ? sddat2.num : -1;
      slot.d3 = sddat3.set ? sddat3.num : -1;
      //slot.cd = sdcd.set ? sdcd.num : -1; // We do CD, and not sure how we would tell it polarity
      slot.width = (sddat2.set && sddat3.set ? 4 : sddat1.set ? 2 : 1);
      if (slot.width == 1)
         slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // Old boards?
      sdmmc_host_t host = SDMMC_HOST_DEFAULT ();
      host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
      host.slot = SDMMC_HOST_SLOT_1;
      esp_vfs_fat_sdmmc_mount_config_t mount_config = {
         .format_if_mount_failed = 1,
         .max_files = 2,
         .allocation_unit_size = 16 * 1024,
         .disk_status_check_enable = 1,
      };
      if (esp_vfs_fat_sdmmc_mount (sd_mount, &host, &slot, &mount_config, &card))
      {
         jo_t j = jo_object_alloc ();
         ESP_LOGE (TAG, "SD Mount failed");
         jo_string (j, "error", "Failed to mount");
         revk_error ("SD", &j);
         card = NULL;
      } else
      {
         esp_vfs_fat_info (sd_mount, &sdsize, &sdfree);
         ESP_LOGE (TAG, "SD Mounted %llu/%llu", sdfree, sdsize);
      }
   }
   gfx_lock ();
   gfx_clear (0);
   for (int y = 0; y < gfx_height (); y++)
      for (int x = (y & 1); x < gfx_width (); x += 2)
         gfx_pixel (x, y, 255);
   gfx_refresh ();
   gfx_unlock ();
   gfx_lock ();
   gfx_clear (0);
   for (int y = 0; y < gfx_height (); y++)
      for (int x = 1 - (y & 1); x < gfx_width (); x += 2)
         gfx_pixel (x, y, 255);
   gfx_refresh ();
   gfx_unlock ();
   uint32_t fresh = 0;
   uint32_t min = 0;
   uint8_t reshow = 0;
   while (1)
   {
      usleep (100000);
      time_t now = time (0);
      if (now < 1000000000)
         now = 0;
      uint32_t up = uptime ();
      if (b.wificonnect)
      {
         gfx_refresh ();
         b.startup = 1;
         b.wificonnect = 0;
         if (startup)
         {
            char msg[1000];
            char *p = msg;
            char temp[32];
            char *qr1 = NULL,
               *qr2 = NULL;
            p += sprintf (p, "[-6]%.16s/%.16s/[3]%s %s/", appname, hostname, revk_version, revk_build_date (temp) ? : "?");
            if (sta_netif)
            {
               wifi_ap_record_t ap = {
               };
               esp_wifi_sta_get_ap_info (&ap);
               if (*ap.ssid)
               {
                  override = up + startup;
                  p += sprintf (p, "[3] /[6] WiFi/[-6]%.32s/[3] /Channel %d/RSSI %d/", (char *) ap.ssid, ap.primary, ap.rssi);
                  {
                     esp_netif_ip_info_t ip;
                     if (!esp_netif_get_ip_info (sta_netif, &ip) && ip.ip.addr)
                        p += sprintf (p, "[6] /IPv4/" IPSTR "/", IP2STR (&ip.ip));
                     asprintf (&qr2, "http://" IPSTR "/", IP2STR (&ip.ip));
                  }
#ifdef CONFIG_LWIP_IPV6
                  {
                     esp_ip6_addr_t ip[LWIP_IPV6_NUM_ADDRESSES];
                     int n = esp_netif_get_all_ip6 (sta_netif, ip);
                     if (n)
                     {
                        p += sprintf (p, "[6] /IPv6/[2]");
                        char *q = p;
                        for (int i = 0; i < n && i < 4; i++)
                           if (n == 1 || ip[i].addr[0] != 0x000080FE)   // Yeh FE80 backwards
                              p += sprintf (p, IPV6STR "/", IPV62STR (ip[i]));
                        while (*q)
                        {
                           *q = toupper (*q);
                           q++;
                        }
                     }
                  }
#endif
               }
            }
            if (!override && ap_netif)
            {
               uint8_t len = revk_wifi_is_ap (temp);
               if (len)
               {
                  override = up + (aptime ? : 600);
                  p += sprintf (p, "[3] /[6]WiFi[-3]%.*s/", len, temp);
                  if (*appass)
                     asprintf (&qr1, "WIFI:S:%.*s;T:WPA2;P:%s;;", len, temp, appass);
                  else
                     asprintf (&qr1, "WIFI:S:%.*s;;", len, temp);
                  {
                     esp_netif_ip_info_t ip;
                     if (!esp_netif_get_ip_info (ap_netif, &ip) && ip.ip.addr)
                     {
                        p += sprintf (p, "[6] /IPv4/" IPSTR "/ /", IP2STR (&ip.ip));
                        asprintf (&qr2, "http://" IPSTR "/", IP2STR (&ip.ip));
                     }
                  }
               }
            }
            if (override)
            {
               if (sdsize)
                  p += sprintf (p, "/ /[2]SD free %lluG of %lluG/ /", sdfree / 1000000000ULL, sdsize / 1000000000ULL);
               ESP_LOGE (TAG, "%s", msg);
               gfx_lock ();
               gfx_message (msg);
               int max = gfx_height () - gfx_y ();
               if (max > gfx_width () / 2)
                  max = gfx_width () / 2;
               if (qr1)
               {
                  gfx_pos (0, gfx_height () - 1, GFX_L | GFX_B);
                  gfx_qr (qr1, max);
               }
               if (qr2)
               {
                  gfx_pos (gfx_width () - 1, gfx_height () - 1, GFX_R | GFX_B);
                  gfx_qr (qr2, max);
               }
               gfx_unlock ();
               reshow = gfxrepeat;
            }
            free (qr1);
            free (qr2);
         }
      }
      if (override)
      {
         if (override < up)
            min = override = 0;
         else
         {
            if (reshow)
            {
               reshow--;
               gfx_force ();
            }
            continue;
         }
      }
      if (!b.startup || (now / 60 == min && !b.redraw))
         continue;              // Check / update every minute
      if (now / 60 != min && reshow < 3)
         reshow = gfxrepeat;
      min = now / 60;
      struct tm t;
      localtime_r (&now, &t);
      if (*lights && !b.lightoverride)
      {
         int hhmm = t.tm_hour * 100 + t.tm_min;
         showlights (lighton == lightoff || (lighton < lightoff && lighton <= hhmm && lightoff > hhmm)
                     || (lightoff < lighton && (lighton <= hhmm || lightoff > hhmm)) ? lights : "");
      }
      b.redraw = 0;
      // Image
      gfx_lock ();
      if (reshow)
         reshow--;
      if (refresh && now / refresh != fresh)
      {                         // Periodic refresh, e.g.once a day
         fresh = now / refresh;
         gfx_refresh ();
      }
      gfx_clear (0);
      for (int w = 0; w < WIDGETS; w++)
      {
         gfx_align_t a = 0;
         if (widgeth[w] <= REVK_SETTINGS_WIDGETH_CENTRE)
            a |= GFX_L;
         if (widgeth[w] >= REVK_SETTINGS_WIDGETH_CENTRE)
            a |= GFX_R;
         if (widgetv[w] <= REVK_SETTINGS_WIDGETV_MIDDLE)
            a |= GFX_T;
         if (widgetv[w] >= REVK_SETTINGS_WIDGETV_MIDDLE)
            a |= GFX_B;
         gfx_pos (widgetx[w], widgety[w], a);
         gfx_colour (widgetk[w] == REVK_SETTINGS_WIDGETK_INVERT || widgetk[w] == REVK_SETTINGS_WIDGETK_MASKINVERT ? 'W' : 'K');
         gfx_background (widgetk[w] == REVK_SETTINGS_WIDGETK_NORMAL || widgetk[w] == REVK_SETTINGS_WIDGETK_MASK ? 'W' : 'K');
         switch (widgett[w])
         {
         case REVK_SETTINGS_WIDGETT_TEXT:
            break;
         case REVK_SETTINGS_WIDGETT_IMAGE:
            if (*widgetc[w])
            {
               image_t *i = download (widgetc[w]);
               if (i && i->size && i->w && i->h)
               {
                  plot_t settings = { 0 };
                  gfx_draw (i->w, i->h, 0, 0, &settings.ox, &settings.oy);
                  ESP_LOGE (TAG, "ox=%d oy=%d f=%c b=%c a=%02X", settings.ox, settings.oy, gfx_f (), gfx_b (), gfx_a ());
                  lwpng_t *p = lwpng_init (&settings, NULL, &plot, &my_alloc, &my_free, NULL);
                  lwpng_data (p, i->size, i->data);
                  const char *e = lwpng_end (&p);
                  if (e)
                     ESP_LOGE (TAG, "PNG fail %s", e);
               }
            }
            break;
         case REVK_SETTINGS_WIDGETT_QR:
            break;
         case REVK_SETTINGS_WIDGETT_CLOCK:
            break;
         }
      }
      gfx_unlock ();
      if (reshow)
         b.redraw = 1;
   }
}

void
revk_web_extra (httpd_req_t * req, int page)
{
   if (!page)
   {
      revk_web_setting_title (req, "Main image settings");
      revk_web_setting_info (req,
                             "Background image at URL should be 1 bit per pixel raw data for the image. See <a href='https://github.com/revk/ESP32-RevK/blob/master/Manuals/Seasonal.md'>season code</a>.");
      revk_web_setting (req, "Startup", "startup");
      revk_web_setting (req, "Image invert", "gfxinvert");
      if (rgb.set && leds > 1)
      {
         revk_web_setting_title (req, "LEDs");
         revk_web_setting (req, "Light pattern", "lights");
         revk_web_setting (req, "Light on", "lighton");
         revk_web_setting (req, "Light off", "lightoff");
      }
      return;
   }
   revk_web_setting_title (req, "Widget settings %d", page);
   revk_web_setting_info (req, "This build is for a display %dx%d pixels", gfx_width (), gfx_height ());
   void add (const char *pre, const char *tag)
   {
      char name[20];
      sprintf (name, "%s%d", tag, page);
      revk_web_setting (req, pre, name);
   }
   add (NULL, "widgett");
   add (NULL, "widgetk");
   add (NULL, "widgetx");
   add (NULL, "widgeth");
   add (NULL, "widgety");
   add (NULL, "widgetv");
   const char *p = NULL;
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_IMAGE)
      add (p, "widgets");
   p = NULL;
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE)
      p = "PNG Image URL";
   add (p, "widgetc");
}
