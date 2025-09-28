/* EPDapp */
/* Copyright ©2019 - 2025 Adrian Kennard, Andrews & Arnold Ltd.See LICENCE file for details .GPL 3.0 */

static const char TAG[] = "EPD";

//#define       TIMINGS

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
#include <driver/mcpwm_prelude.h>
#include <driver/i2s_pdm.h>
#include "gfx.h"
#include "iec18004.h"
#include <hal/spi_types.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <onewire_bus.h>
#include <ds18b20.h>
#include <lwpng.h>
#include "EPD.h"
#include <math.h>
#include <halib.h>
#include <ir.h>
#include "bleenv.h"

#define	LEFT	0x80            // Flags on font size
#define	RIGHT	0x40
#define	LINE	0x20
#define	MASK	0x1F
#define MINSIZE	4

#ifndef	GFX_EPD
#define	gfxinvert	0
#endif

const char sd_mount[] = "/sd";
uint64_t sdsize = 0,            // SD card data
   sdfree = 0;

char season = 0;                // Current season

static struct
{                               // Flags
   uint8_t die:1;
   uint8_t wificonnect:1;
   uint8_t redraw:1;
   uint8_t setting:1;
   uint8_t lightoverride:1;
   uint8_t startup:1;
   uint8_t ha:1;
   uint8_t bl:1;
   uint8_t defcon:3;
} volatile b = { 0 };

const char *const btns[] = { "up", "down", "left", "right", "push" };
revk_gpio_t btng[sizeof (btns) / sizeof (*btns)] = { 0 };

#define LONG_PRESS	200

volatile uint32_t override = 0;
volatile char *overrideimage = NULL;
volatile char *overridemessage = NULL;

#define BL_TIMEBASE_RESOLUTION_HZ 1000000       // 1MHz, 1us per tick
#define BL_TIMEBASE_PERIOD        1000
uint8_t bl = 0;

#ifdef	CONFIG_REVK_SOLAR
time_t sunrise = 0,
   sunset = 0;
uint16_t sunrisehhmm = 0,
   sunsethhmm = 0;
#endif

#ifdef	GFX_EPD
const char hhmm[] = "%H:%M";
#else
const char hhmm[] = "%T";
#endif

static int8_t i2cport = 0;
jo_t mqttjson[sizeof (jsonsub) / sizeof (*jsonsub)] = { 0 };

void revk_state_extra (jo_t j);

jo_t weather = NULL;
jo_t apijson = NULL;
jo_t solar = NULL;
jo_t veml6040 = NULL;
jo_t mcp9808 = NULL;
jo_t gzp6816d = NULL;
jo_t t6793 = NULL;
jo_t scd41 = NULL;
uint16_t scd41to = 0;
uint32_t scd41_serial = 0;
jo_t tmp1075 = NULL;
jo_t sht40 = NULL;
uint32_t sht40_serial = 0;
jo_t ds18b20s = NULL;
uint8_t ds18b20_num = 0;
jo_t bles = NULL;
jo_t noise = NULL;

struct
{
   int sock;                    // UDP socket
   socklen_t addrlen;           // Address len
   struct sockaddr addr;        // Address
   uint32_t id;                 // SNMP ID field to check
   time_t upfrom;               // Back tracked uptime start
   uint32_t lasttx;             // Last packet tx time (uptime secs)
   uint32_t lastrx;             // Last packet rx time (uptime secs)
   char *host;                  // Hostname
   char *desc;                  // Description
} snmp = { 0 };

// TODO snmp as a JSON object with data

httpd_handle_t webserver = NULL;

led_strip_handle_t strip = NULL;
sdmmc_card_t *card = NULL;

SemaphoreHandle_t epd_mutex = NULL;
SemaphoreHandle_t json_mutex = NULL;
SemaphoreHandle_t file_mutex = NULL;

static void
json_store (jo_t * jp, jo_t j)
{
   xSemaphoreTake (json_mutex, portMAX_DELAY);
   jo_free (jp);
   *jp = j;
   jo_rewind (j);
   xSemaphoreGive (json_mutex);
}

const char *
gfx_qr (const char *value, uint16_t max)
{
#ifndef	CONFIG_GFX_BUILD_SUFFIX_GFXNONE
   unsigned int width = 0;
   uint8_t *qr = qr_encode (strlen (value), value,.widthp = &width,.noquiet = (max & 0x4000 ? 1 : 0));
   if (!qr)
      return "Failed to encode";
   uint8_t special = 0;
   if (max & 0x2000)
      special = 1;
   if (max & 0x8000)
      max = width * (max & 0xFFF);
   max &= 0xFFF;
   if (!max)
      max = width;
   if (max < width)
   {
      free (qr);
      return "No space";
   }
   if (max > gfx_width () || max > gfx_height ())
   {
      free (qr);
      return "Too big";
   }
   int s = max / width;
   gfx_pos_t ox,
     oy;
   gfx_draw (max, max, 0, 0, &ox, &oy);
   int d = (max - width * s) / 2;
   ox += d;
   oy += d;
   int s2 = s * s;
   if (s < 3)
      special = 0;
   if (s >= 3)
      s2--;
   if (s >= 8)
      s2--;
   // Some time we should change this to use the run plotting and anti-aliasing
   for (int y = 0; y < width; y++)
      for (int x = 0; x < width; x++)
      {
         uint8_t b = qr[width * y + x];
         if (!special || !(b & QR_TAG_BLACK) || (b & QR_TAG_TARGET))
            for (int dy = 0; dy < s; dy++)      // box
               gfx_pixel_fb_run (ox + x * s, oy + y * s + dy, b & QR_TAG_BLACK ? 0xFF : 0, s);
         else
            for (int dy = 0; dy < s; dy++)      // dot
               for (int dx = 0; dx < s; dx++)
                  gfx_pixel_fb (ox + x * s + dx, oy + y * s + dy,
                                ((dx * 2 + 1 - s) * (dx * 2 + 1 - s) + (dy * 2 + 1 - s) * (dy * 2 + 1 - s) < s2) ? 255 : 0);
      }
   free (qr);
#endif
   return NULL;
}

const char *const longday[] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };
const char *const shortday[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

time_t
parse_time (const char *t, int thisyear)
{
   struct tm tm = { 0 };
   int y = 0,
      m = 0,
      d = 0,
      H = 0,
      M = 0,
      S = 0;
   if (!t || sscanf (t, "%d-%d-%d %d:%d:%d", &y, &m, &d, &H, &M, &S) < 3)
      return 0;
   if (!y)
      y = thisyear;
   tm.tm_year = y - 1900;
   tm.tm_mon = m - 1;
   tm.tm_mday = d;
   tm.tm_hour = H;
   tm.tm_min = M;
   tm.tm_sec = S;
   tm.tm_isdst = -1;
   time_t T = mktime (&tm);
   if (T == -1)
      T = 0;
   return T;
}

void
showlights (const char *rgb)
{
   if (!strip)
      return;
   const char *c = rgb;
   for (int i = 0; i < lightcount; i++)
   {
      uint8_t mode = 0;
      if (i < sizeof (lightmode) / sizeof (*lightmode))
         mode = lightmode[i];
      switch (mode)
      {
      case REVK_SETTINGS_LIGHTMODE_RGB_PATTERN:
         revk_led (strip, i, 255, revk_rgb (*c));
         if (*c)
            c++;
         if (!*c)
            c = rgb;
         break;
      case REVK_SETTINGS_LIGHTMODE_STATUS:     // Done in task
         break;
      case REVK_SETTINGS_LIGHTMODE_OFF:
         revk_led (strip, i, 255, 0);
         break;
      }
   }
}

void
setlights (const char *rgb)
{
   if (rgb)
   {
      showlights (rgb);
      b.lightoverride = 1;
   } else
   {
      showlights ("");
      b.lightoverride = 0;
   }
}

void
defcon_cb (void *arg, const char *topic, jo_t j)
{                               // DEFCON state
   char *value = jo_strdup (j);
   if (!value)
      return;
   int level = 0;
   if (!strncmp (topic, "DEFCON/", 7) && isdigit ((int) (uint8_t) topic[7]))
      level = topic[7] - '0';
   // With value it is used to turn on/off a defcon state, the lowest set dictates the defcon level
   // With no value, this sets the DEFCON state directly instead of using lowest of state set
   static uint8_t state = 0;    // DEFCON state
   if (*value)
   {
      if (*value == '1' || *value == 't' || *value == 'y')
         state |= (1 << level);
      else
         state &= ~(1 << level);
      int l;
      for (l = 0; l < 7 && !(state & (1 << l)); l++);
      level = l;
   }
   if (b.defcon != level)
   {
      b.defcon = level;
      b.redraw = 1;
   }
   free (value);
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
   if (!strcasecmp (suffix, "override") && *value)
   {
      char *was = (void *) overrideimage;
      if (was)
         return "Too quick";
      overrideimage = NULL;
      free (was);
      overrideimage = strdup (value);
      return "";
   }
   if (!strcmp (suffix, "message") && *value)
   {
      char *was = (void *) overridemessage;
      if (was)
         return "Too quick";
      overridemessage = NULL;
      free (was);
      overridemessage = strdup (value);
      return "";
   }
   const char *log_json (jo_t * jp)
   {
      const char *e = "No data";
      xSemaphoreTake (json_mutex, portMAX_DELAY);
      if (*jp)
      {
         jo_t j = jo_copy (*jp);
         e = revk_info (suffix, &j) ? : "";
      }
      xSemaphoreGive (json_mutex);
      return e;
   }
   if (!strcasecmp (suffix, "weather"))
      return log_json (&weather);
   if (!strcasecmp (suffix, "noise"))
      return log_json (&noise);
   if (!strcasecmp (suffix, "api"))
      return log_json (&apijson);
   if (!strncasecmp (suffix, "mqtt", 4) && isdigit ((int) (uint8_t) suffix[4]) && !suffix[5] && suffix[4] > '0'
       && suffix[4] <= '0' + sizeof (mqttjson) / sizeof (*mqttjson))
      return log_json (&mqttjson[suffix[4] - '1']);
   if (!strcasecmp (suffix, "solar"))
      return log_json (&solar);
   if (!strcasecmp (suffix, "veml6040"))
      return log_json (&veml6040);
   if (!strcasecmp (suffix, "mcp9808"))
      return log_json (&mcp9808);
   if (!strcasecmp (suffix, "gzp6816d"))
      return log_json (&gzp6816d);
   if (!strcasecmp (suffix, "t6793"))
      return log_json (&t6793);
   if (!strcasecmp (suffix, "scd41"))
      return log_json (&scd41);
   if (!strcasecmp (suffix, "sht40"))
      return log_json (&sht40);
   if (!strcasecmp (suffix, "tmp1075"))
      return log_json (&tmp1075);
   if (!strcasecmp (suffix, "ds18b20"))
      return log_json (&ds18b20s);
   if (!strcasecmp (suffix, "ble"))
      return log_json (&bles);
   if (!strcmp (suffix, "bl"))
   {
      bl = atoi (value);
      return "";
   }
   if (!strcmp (suffix, "setting"))
   {
      b.setting = 1;
      snmp.lasttx = 0;          // Force re-lookup
      return "";
   }
   if (!strcmp (suffix, "connect"))
   {
      b.ha = 1;
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

file_t *files = NULL;

file_t *
find_file (char *url, const char *suffix)
{
   xSemaphoreTake (file_mutex, portMAX_DELAY);
   file_t *i;
   for (i = files; i && (strcmp (i->url, url) || strcmp (i->suffix ? : "", suffix ? : "")); i = i->next);
   if (!i)
   {
      i = mallocspi (sizeof (*i));
      if (i)
      {
         memset (i, 0, sizeof (*i));
         i->url = strdup (url);
         i->suffix = suffix;
         i->next = files;
         files = i;
      }
   }
   xSemaphoreGive (file_mutex);
   return i;
}

void
check_file (file_t * i)
{                               // In mutex
   if (!i || !i->data || !i->size)
      return;
   i->changed = uptime ();
   const char *e1 = lwpng_get_info (i->size, i->data, &i->w, &i->h);
   if (!e1)
   {
      i->json = 0;              // PNG
      ESP_LOGE (TAG, "Image %s len %lu width %lu height %lu cached %lu", i->url, i->size, i->w, i->h, i->cachetime);
   } else
   {                            // Not a png
      jo_t j = jo_parse_mem (i->data, i->size);
      jo_skip (j);
      const char *e2 = jo_error (j, NULL);
      jo_free (&j);
      if (!e2)
      {                         // Valid JSON
         i->json = 1;
         i->w = i->h = 0;
         ESP_LOGE (TAG, "JSON %s len %lu cached %lu", i->url, i->size, i->cachetime);
      } else
      {                         // Not sensible
         free (i->data);
         i->data = NULL;
         i->size = 0;
         i->w = i->h = 0;
         i->changed = 0;
         ESP_LOGE (TAG, "Unknown %s error %s %s", i->url, e1 ? : "", e2 ? : "");
      }
   }
   i->updated = 1;
}

file_t *
download (char *url, const char *suffix, uint8_t force, uint32_t cache)
{
   uint32_t up = uptime ();
   file_t *i = find_file (url, suffix);
   if (!i)
      return i;
   suffix = i->suffix;
   if (!suffix || strchr (url, '?'))
      suffix = "";
   else
   {
      char *p = url + strlen (url);;
      while (p > url && isalnum ((int) (uint8_t) p[-1]))
         p--;
      if (p > url && p[-1] == '.')
         suffix = "";
   }
   if (!*baseurl || !strncasecmp (i->url, "http://", 7) || !strncasecmp (i->url, "https://", 8))
      asprintf (&url, "%s%s", i->url, suffix);
   else
   {                            // Prefix URL
      int l = strlen (baseurl);
      if (baseurl[l - 1] == '/')
         l--;
      asprintf (&url, "%.*s/%s%s", l, baseurl, i->url, suffix);
   }
   int32_t len = 0;
   uint8_t *buf = NULL;
   int response = -1;
   i->cachetime = cache;        // for reload
   if (i->cached && !force)
   {
      if (i->cached < up)
         i->reload = 1;
      response = (i->data ? 304 : 404); // Cached
   } else if (!revk_link_down () && (!strncasecmp (url, "http://", 7) || !strncasecmp (url, "https://", 8)))
   {
      esp_http_client_config_t config = {
         .url = url,
         .crt_bundle_attach = esp_crt_bundle_attach,
         .timeout_ms = 20000,
      };
      i->cached = up + cache;
      i->reload = 0;
      esp_http_client_handle_t client = esp_http_client_init (&config);
      if (client)
      {
         if (i->changed)
         {
            char when[50];
            time_t t = time (0) + i->changed - up;
            struct tm tm;
            gmtime_r (&t, &tm);
            strftime (when, sizeof (when), "%a, %d %b %Y %T GMT", &tm);
            esp_http_client_set_header (client, "If-Modified-Since", when);
         }
         if (!esp_http_client_open (client, 0))
         {
            len = esp_http_client_fetch_headers (client);
            ESP_LOGD (TAG, "%s Len %ld", url, len);
            if (!len)
            {                   // Dynamic, FFS
               size_t l;
               FILE *o = open_memstream ((char **) &buf, &l);
               if (o)
               {
                  char temp[64];
                  while ((len = esp_http_client_read (client, temp, sizeof (temp))) > 0)
                     fwrite (temp, len, 1, o);
                  fclose (o);
                  len = l;
               }
               if (!buf)
                  len = 0;
            } else
            {
               buf = mallocspi (len);
               if (buf)
                  len = esp_http_client_read_response (client, (char *) buf, len);
            }
            response = esp_http_client_get_status_code (client);
            if (response == 200 || response == 304)
               i->backoff = 0;
            else
               ESP_LOGE (TAG, "%s response %d cached %ld", url, response, i->cached - up);
            esp_http_client_close (client);
         } else
            ESP_LOGE (TAG, "Failed http client open %s", url);
         esp_http_client_cleanup (client);
      } else
         ESP_LOGE (TAG, "Failed http client %s", url);
      ESP_LOGD (TAG, "Got %s %d", url, response);
   }
   if (response != 304)
   {
      if (response != 200)
      {                         // Failed
         if (2 * i->backoff > 255)
            i->backoff = 255;
         else
            i->backoff = (i->backoff * 2 ? : 1);
         i->cached = up + i->backoff;
         jo_t j = jo_object_alloc ();
         jo_string (j, "url", url);
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
            xSemaphoreTake (file_mutex, portMAX_DELAY);
            free (i->data);
            i->data = buf;
            i->size = len;
            check_file (i);
            xSemaphoreGive (file_mutex);
         }
         buf = NULL;
      }
   }
   if (card && !i->json)
   {                            // SD (JSON is assumed dynamic and not saved, pngs we assume sensible to cache or read from card)
      char *s = strrchr (url, '/');
      if (!s)
         s = url;
      if (s)
      {
         char *fn = NULL;
         if (*s == '/')
            s++;
         asprintf (&fn, "%s/%s", sd_mount, s);
         char *q = fn + sizeof (sd_mount);
         while (*q && (isalnum ((int) (uint8_t) * q) || *q == '_' || *q == '-'))
            q++;
         if (*q == '.')
         {
            q++;
            while (*q && isalnum ((int) (uint8_t) * q))
               q++;
         }
         *q = 0;
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
               ESP_LOGE (TAG, "Write %s %lu", fn, i->size);
            } else
               ESP_LOGE (TAG, "Write fail %s", fn);
         } else if (!i->card && (!i->data || (response && response != 304 && response != -1)))
         {                      // Load from card
            i->card = 1;        // card tried, no need to try again
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
                        xSemaphoreTake (file_mutex, portMAX_DELAY);
                        free (i->data);
                        i->data = buf;
                        i->size = s.st_size;
                        check_file (i);
                        xSemaphoreGive (file_mutex);
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
   free (url);
   return i;
}

// Image plot

typedef struct plot_s
{
   gfx_pos_t ox,
     oy;
   uint8_t invert;
} plot_t;

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

static const char *
pixel (void *opaque, uint32_t x, uint32_t y, uint16_t r, uint16_t g, uint16_t b, uint16_t a)
{
   plot_t *p = opaque;
   if (p->invert)
   {                            // Gets inverted to reverse before
      r ^= 0xFFFF;
      g ^= 0xFFFF;
      b ^= 0xFFFF;
   }
   gfx_pixel_argb (p->ox + x, p->oy + y, ((a >> 8) << 24) | ((r >> 8) << 16) | ((g >> 8) << 8) | (b >> 8));
   return NULL;
}

void
plot (file_t * i, gfx_pos_t ox, gfx_pos_t oy, uint8_t invert)
{
   xSemaphoreTake (file_mutex, portMAX_DELAY);
   plot_t settings = { ox, oy, invert };
   lwpng_decode_t *p = lwpng_decode (&settings, NULL, &pixel, &my_alloc, &my_free, NULL);
   lwpng_data (p, i->size, i->data);
   const char *e = lwpng_decoded (&p);
   xSemaphoreGive (file_mutex);
   if (e)
      ESP_LOGE (TAG, "PNG fail %s", e);
}

#ifdef	CONFIG_REVK_APCONFIG
#error 	Clash with CONFIG_REVK_APCONFIG set
#endif

static void
register_uri (const httpd_uri_t * uri_struct)
{
   esp_err_t res = httpd_register_uri_handler (webserver, uri_struct);
   if (res != ESP_OK)
   {
      ESP_LOGE (TAG, "Failed to register %s, error code %d", uri_struct->uri, res);
   }
}

static void
register_get_uri (const char *uri, esp_err_t (*handler) (httpd_req_t * r))
{
   httpd_uri_t uri_struct = {
      .uri = uri,
      .method = HTTP_GET,
      .handler = handler,
   };
   register_uri (&uri_struct);
}

static esp_err_t
web_status (httpd_req_t * req)
{
   jo_t j = jo_object_alloc ();
   revk_state_extra (j);
   char *js = jo_finisha (&j);
   httpd_resp_set_type (req, "application/json");
   httpd_resp_send (req, js, strlen (js));
   return ESP_OK;
}

static void
settings_ble (httpd_req_t * req, int i)
{
   revk_web_send (req, "<tr><td>BLE[%d]</td><td>"       //
                  "<select name=blesensor%d>", i, i + 1);
   revk_web_send (req, "<option value=\"\">-- None --");
   char found = 0;
   for (bleenv_t * e = bleenv; e; e = e->next)
   {
      revk_web_send (req, "<option value=\"%s\"", e->mac);
      if (*blesensor[i] && (!strcmp (blesensor[i], e->name) || !strcmp (blesensor[i], e->mac)))
      {
         revk_web_send (req, " selected");
         found = 1;
      }
      revk_web_send (req, ">%s", e->name);
      if (!e->missing && e->rssi)
         revk_web_send (req, " %ddB", e->rssi);
      if (!e->missing && e->tempset)
         revk_web_send (req, " %.2f°", (float) e->temp / 100);
   }
   if (!found && *blesensor[i])
      revk_web_send (req, "<option selected value=\"%s\">%s", blesensor[i], blesensor[i]);
   revk_web_send (req, "</select>");
   revk_web_send (req, "</td><td>External BLE temperature reference</td></tr>");
}

static esp_err_t
web_root (httpd_req_t * req)
{
   if (revk_link_down ())
      return revk_web_settings (req);   // Direct to web set up
   revk_web_head (req, hostname);
   char *qs = NULL;
   revk_web_send (req, "<h1>%s</h1>", revk_web_safe (&qs, hostname));
   free (qs);
#ifndef	CONFIG_GFX_BUILD_SUFFIX_GFXNONE
#ifdef	CONFIG_LWPNG_ENCODE
   revk_web_send (req, "<p>");
   int32_t w = gfx_width ();
   int32_t h = gfx_height ();
   int DIV = gfx_width () / 200 ? : 1;
   uint8_t f = gfx_flip ();
   revk_web_send (req, "<div style='display:inline-block;width:%dpx;height:%dpx;margin:5px;border:10px solid %s;border-%s:%dpx solid %s;'><img width=%d height=%d src='frame.png' style='transform:",   //
                  w / DIV, h / DIV,     //
#ifdef	GFX_LCD
                  "black",      //
#else
                  gfxinvert ? "black" : "white",        //
#endif
                  f & 4 ? f & 2 ? "left" : "right" : f & 2 ? "top" : "bottom",  //
#ifdef	GFX_LCD
                  10, "black",  //
#else
                  20, gfxinvert ? "black" : "white",    //
#endif
                  gfx_raw_w () / DIV, gfx_raw_h () / DIV        //
      );
   if (f & 4)
      revk_web_send (req, "translate(%dpx,%dpx)rotate(90deg)scale(1,-1)",       //
                     (w - h) / 2 / DIV, (h - w) / 2 / DIV);
   revk_web_send (req, "scale(%d,%d);'></div>", f & 1 ? -1 : 1, f & 2 ? -1 : 1  //
      );
#undef DIV
   revk_web_send (req, "</p><p><a href=/>Reload</a></p>");
#endif
#endif
   return revk_web_foot (req, 0, 1, NULL);
}

#ifdef	GFX_COLOUR
static gfx_colour_t
contrast (gfx_colour_t c)
{                               // Black or white foreground
   if ((((c >> 16) & 255) * 2 + ((c >> 8) & 255) * 3 + (c & 255)) >= 255 * 3)
      return 0;
   return 0xFFFFFF;
}
#endif

void
epd_lock (void)
{
   xSemaphoreTake (epd_mutex, portMAX_DELAY);
   gfx_lock ();
#ifdef GFX_COLOUR
   gfx_colour_t c = gfx_rgb (*background);
   gfx_background (c);
   gfx_foreground (contrast (c));
#endif
}

void
epd_unlock (void)
{
   gfx_unlock ();
   xSemaphoreGive (epd_mutex);
}

#ifdef	CONFIG_LWPNG_ENCODE
static esp_err_t
web_frame (httpd_req_t * req)
{
   xSemaphoreTake (epd_mutex, portMAX_DELAY);
   uint8_t *png = NULL;
   size_t len = 0;
   uint32_t w = gfx_raw_w ();
   uint32_t h = gfx_raw_h ();
   uint8_t *b = gfx_raw_b ();
   const char *e = NULL;
   if (gfx_bpp () == 1)
   {
      lwpng_encode_t *p = lwpng_encode_1bit (w, h, &my_alloc, &my_free, NULL);
      if (b)
         while (h--)
         {
            lwpng_encode_scanline (p, b);
            b += (w + 7) / 8;
         }
      e = lwpng_encoded (&p, &len, &png);
   } else if (gfx_bpp () == 2)
   {                            // Black/White/Red
#if 0
      // TODO
      uint8_t *buf = mallocspi (w * 3);
      if (!buf)
         e = "malloc";
      else
      {
         lwpng_encode_t *p = lwpng_encode_2bit (w, h, &my_alloc, &my_free, NULL);
         if (b)
            while (h--)
            {
               lwpng_encode_scanline (p, b);
               b += (w + 7) / 4;
            }
         e = lwpng_encoded (&p, &len, &png);
         free (buf);
      }
#else
      e = "No 2 bit coding yet";
#endif
   } else if (gfx_bpp () == 8)
   {                            // Grey
      lwpng_encode_t *p = lwpng_encode_grey (w, h, &my_alloc, &my_free, NULL);
      if (b)
         while (h--)
         {
            lwpng_encode_scanline (p, b);
            b += w;
         }
      e = lwpng_encoded (&p, &len, &png);
   } else if (gfx_bpp () == 16)
   {                            // RGB packed
      uint8_t *buf = mallocspi (w * 3);
      if (!buf)
         e = "malloc";
      else
      {
         lwpng_encode_t *p = lwpng_encode_rgb (w, h, &my_alloc, &my_free, NULL);
         if (b)
            while (h--)
            {
               for (int x = 0; x < w; x++)
               {
                  uint16_t v = (b[x * 2 + 0] << 8) | b[x * 2 + 1];
                  uint8_t r = (v >> 11) << 3;
                  r += (r >> 5);
                  uint8_t g = ((v >> 5) & 0x3F) << 2;
                  g += (g >> 6);
                  uint8_t b = (v & 0x1F) << 3;
                  b += (b >> 5);
                  buf[x * 3 + 0] = r;
                  buf[x * 3 + 1] = g;
                  buf[x * 3 + 2] = b;
               }
               lwpng_encode_scanline (p, buf);
               b += w * 2;
            }
         e = lwpng_encoded (&p, &len, &png);
         free (buf);
      }
   }
   ESP_LOGD (TAG, "Encoded %u bytes %s", len, e ? : "");
   if (e)
   {
      revk_web_head (req, *hostname ? hostname : appname);
      revk_web_send (req, e);
      revk_web_foot (req, 0, 1, NULL);
   } else
   {
      httpd_resp_set_type (req, "image/png");
      httpd_resp_send (req, (char *) png, len);
   }
   free (png);
   xSemaphoreGive (epd_mutex);
   return ESP_OK;
}
#endif

void
snmp_tx (void)
{                               // Send an SNMP if needed
   if (!*snmphost)
      return;
   uint32_t up = uptime ();
   if (snmp.lasttx && snmp.lasttx + 60 > up)
      return;
   if (!snmp.lasttx || snmp.lastrx + 300 > up)
   {                            // Re try socket
      int sock = snmp.sock;
      snmp.sock = -1;
      if (sock >= 0)
         close (sock);
      snmp.lasttx = 0;
   }
   if (snmp.sock < 0)
   {
      const struct addrinfo hints = {
         .ai_family = AF_UNSPEC,
         .ai_socktype = SOCK_DGRAM,
      };
      struct addrinfo *a = 0,
         *t;
      if (!getaddrinfo (snmphost, "161", &hints, &a) && a)
      {
         for (t = a; t; t = t->ai_next)
         {
            int sock = socket (t->ai_family, t->ai_socktype, t->ai_protocol);
            if (sock >= 0)
            {
               memcpy (&snmp.addr, t->ai_addr, snmp.addrlen = t->ai_addrlen);
               snmp.id = ((esp_random () & 0x7FFFFF7F) | 0x40000040);   // bodge to ensure 4 bytes
               snmp.sock = sock;
               break;
            }
         }
         freeaddrinfo (a);
      }
   }
   if (snmp.sock < 0)
   {
      ESP_LOGE (TAG, "SNMP lookup fail %s", snmphost);
      return;
   }
   // very crude IPv6 SNMP uptime .1.3.6.1.2.1.1.3.0
   uint8_t payload[] = {        // iso.3.6.1.2.1.1.3.0 iso.3.6.1.2.1.1.5.0 iso.3.6.1.2.1.1.1.0
      0x30, 0x45, 0x02, 0x01, 0x01, 0x04, 0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x38, 0x02,
      0x04, 0x00, 0x00, 0x00, 0x0, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x30, 0x2a, 0x30, 0x0c, 0x06,
      0x08, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x03, 0x00, 0x05, 0x00, 0x30, 0x0c, 0x06, 0x08, 0x2b,
      0x06, 0x01, 0x02, 0x01, 0x01, 0x05, 0x00, 0x05, 0x00, 0x30, 0x0c, 0x06, 0x08, 0x2b, 0x06, 0x01,
      0x02, 0x01, 0x01, 0x01, 0x00, 0x05, 0x00
   };
   *(uint32_t *) (payload + 17) = snmp.id;
   int err = sendto (snmp.sock, payload, sizeof (payload), 0, &snmp.addr, snmp.addrlen);
   if (err < 0)
   {
      ESP_LOGE (TAG, "SNMP Tx fail (sock %d) %s", snmp.sock, esp_err_to_name (err));
      return;
   }
   snmp.lasttx = up;
   ESP_LOGD (TAG, "SNMP Tx");
}

void
bl_task (void *x)
{
   mcpwm_cmpr_handle_t comparator = NULL;
   mcpwm_timer_handle_t bltimer = NULL;
   mcpwm_timer_config_t timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = BL_TIMEBASE_RESOLUTION_HZ,
      .period_ticks = BL_TIMEBASE_PERIOD,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
   };
   mcpwm_new_timer (&timer_config, &bltimer);
   mcpwm_oper_handle_t oper = NULL;
   mcpwm_operator_config_t operator_config = {
      .group_id = 0,            // operator must be in the same group to the timer
   };
   mcpwm_new_operator (&operator_config, &oper);
   mcpwm_operator_connect_timer (oper, bltimer);
   mcpwm_comparator_config_t comparator_config = {
      .flags.update_cmp_on_tez = true,
   };
   mcpwm_new_comparator (oper, &comparator_config, &comparator);
   mcpwm_gen_handle_t generator = NULL;
   mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = gfxbl.num,
      .flags.invert_pwm = gfxbl.invert,
   };
   mcpwm_new_generator (oper, &generator_config, &generator);
   mcpwm_comparator_set_compare_value (comparator, 0);
   mcpwm_generator_set_action_on_timer_event (generator,
                                              MCPWM_GEN_TIMER_EVENT_ACTION (MCPWM_TIMER_DIRECTION_UP,
                                                                            MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
   mcpwm_generator_set_action_on_compare_event
      (generator, MCPWM_GEN_COMPARE_EVENT_ACTION (MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW));
   mcpwm_timer_enable (bltimer);
   mcpwm_timer_start_stop (bltimer, MCPWM_TIMER_START_NO_STOP);
   int b = 0;
   while (1)
   {
      if (b == bl)
      {
         usleep (100000);
         continue;
      }
      if (bl > b)
         b++;
      else
         b--;
      mcpwm_comparator_set_compare_value (comparator, b * BL_TIMEBASE_PERIOD / 100);
      usleep (10000);
   }
}

void
snmp_rx_task (void *x)
{                               // Get SNMP responses
   while (!b.die)
   {
      if (snmp.sock < 0)
      {
         sleep (1);
         continue;
      }
      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      setsockopt (snmp.sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
      uint8_t rx[300];
      struct sockaddr_storage source_addr;
      socklen_t socklen = sizeof (source_addr);
      int len = recvfrom (snmp.sock, rx, sizeof (rx), 0, (struct sockaddr *) &source_addr, &socklen);
      if (len <= 0)
         continue;
      time_t now = time (0);
      if (now < 1000000000)
         now = 0;
      ESP_LOGD (TAG, "SNMP Rx %d", len);
      uint8_t *oid = NULL,
         oidlen = 0,
         resp = 0;
      uint8_t *scan (uint8_t * p, uint8_t * e)
      {
         if (p >= e)
            return NULL;
         uint8_t class = (*p >> 6);
         uint8_t con = (*p & 0x20);
         uint32_t tag = 0;
         if ((*p & 0x1F) != 0x1F)
            tag = (*p & 0x1F);
         else
         {
            do
            {
               p++;
               tag = (tag << 7) | (*p & 0x7F);
            }
            while (*p & 0x80);
         }
         p++;
         if (p >= e)
            return NULL;
         uint32_t len = 0;
         if (*p & 0x80)
         {
            uint8_t b = (*p++ & 0x7F);
            while (b--)
               len = (len << 8) + (*p++);
         } else
            len = (*p++ & 0x7F);
         if (p + len > e)
            return NULL;
         if (con)
         {
            if (tag == 2)
               resp = 1;
            while (p && p < e)
               p = scan (p, e);
            oidlen = 0;
         } else
         {
            int32_t n = 0;
            uint8_t *d = p;
            uint8_t *de = p + len;
            if (!class && tag == 6)
            {
               oid = p;
               oidlen = len;
            }
            if ((!class && tag == 2) || (class == 1 && tag == 3))
            {                   // Int or timeticks
               int s = 1;
               if (*d & 0x80)
                  s = -1;
               n = (*d++ & 0x7F);
               while (d < de)
                  n = (n << 8) + *d++;
               n *= s;
            }
            if (class == 2 && tag == 1 && resp)
            {                   // Response ID (first number in con tag 2)
               resp = 0;
               if (n != snmp.id)
               {
                  ESP_LOGE (TAG, "SNMP Bad ID %08lX expecting %08lX", n, snmp.id);
                  return NULL;
               }
            } else if (class == 1 && tag == 3 && oidlen == 8 && !memcmp (oid, (uint8_t[])
                                                                         {
                                                                         0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x03, 0x00}
                                                                         , 8))
            {
               if (now)
                  snmp.upfrom = now - n / 100;
            } else if (!class && tag == 4 && oidlen == 8 && !memcmp (oid, (uint8_t[])
                                                                     {
                                                                     0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x05, 0x00}
                                                                     , 8))
            {
               char *new = mallocspi (len + 1);
               if (new)
               {
                  char *was = snmp.host;
                  memcpy (new, d, len);
                  new[len] = 0;
                  snmp.host = new;
                  free (was);
               }
            } else if (!class && tag == 4 && oidlen == 8 && !memcmp (oid, (uint8_t[])
                                                                     {
                                                                     0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00}
                                                                     , 8))
            {
               char *new = mallocspi (len + 1);
               if (new)
               {
                  char *was = snmp.desc;
                  memcpy (new, d, len);
                  new[len] = 0;
                  snmp.desc = new;
                  free (was);
               }
            }
            p = de;
         }
         return p;
      }
      scan (rx, rx + len);
      if (snmp.upfrom)
         snmp.lastrx = uptime ();
   }
   vTaskDelete (NULL);
}

void
led_task (void *x)
{
   while (!b.die)
   {
      usleep (100000);
      uint32_t b = revk_blinker ();
      for (int i = 0; i < sizeof (lightmode) / sizeof (*lightmode); i++)
         if (lightmode[i] == REVK_SETTINGS_LIGHTMODE_STATUS)
            revk_led (strip, i, 255, b);
      led_strip_refresh (strip);
   }
   vTaskDelete (NULL);
}

static int32_t
i2c_read_16lh (uint8_t addr, uint8_t cmd)
{
   uint8_t h = 0,
      l = 0;
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, cmd, true);
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_READ, true);
   i2c_master_read_byte (t, &l, I2C_MASTER_ACK);
   i2c_master_read_byte (t, &h, I2C_MASTER_LAST_NACK);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
   {
      ESP_LOGE (TAG, "I2C %02X %02X fail %s", addr & 0x7F, cmd, esp_err_to_name (err));
      return -1;
   }
   ESP_LOGD (TAG, "I2C %02X %02X %02X%02X OK", addr & 0x7F, cmd, h, l);
   return (h << 8) + l;
}

static int32_t
i2c_read_16lh2 (uint8_t addr, uint8_t cmd)
{                               // try twice
   int32_t v = i2c_read_16lh (addr, cmd);
   if (v < 0)
      v = i2c_read_16lh (addr, cmd);
   return v;
}

static esp_err_t
i2c_write_16lh (uint8_t addr, uint8_t cmd, uint16_t val)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, cmd, true);
   i2c_master_write_byte (t, val & 0xFF, true);
   i2c_master_write_byte (t, val >> 8, true);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   return err;
}

static int32_t
i2c_read_16hl (uint8_t addr, uint8_t cmd)
{
   uint8_t h = 0,
      l = 0;
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, cmd, true);
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_READ, true);
   i2c_master_read_byte (t, &h, I2C_MASTER_ACK);
   i2c_master_read_byte (t, &l, I2C_MASTER_LAST_NACK);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
   {
      ESP_LOGE (TAG, "I2C %02X %02X fail %s", addr & 0x7F, cmd, esp_err_to_name (err));
      return -1;
   }
   ESP_LOGD (TAG, "I2C %02X %02X %02X%02X OK", addr & 0x7F, cmd, h, l);
   return (h << 8) + l;
}

static esp_err_t
i2c_write_16hl (uint8_t addr, uint8_t cmd, uint16_t val)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, cmd, true);
   i2c_master_write_byte (t, val >> 8, true);
   i2c_master_write_byte (t, val & 0xFF, true);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   return err;
}


static int32_t
i2c_modbus_read (uint8_t addr, uint16_t a)
{
   uint8_t s = 0,
      b = 0,
      h = 0,
      l = 0;
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, 0x04, true);
   i2c_master_write_byte (t, a >> 8, true);
   i2c_master_write_byte (t, a, true);
   i2c_master_write_byte (t, 0x00, true);
   i2c_master_write_byte (t, 0x01, true);
   i2c_master_start (t);
   i2c_master_write_byte (t, (addr << 1) | I2C_MASTER_READ, true);
   i2c_master_read_byte (t, &s, I2C_MASTER_ACK);
   i2c_master_read_byte (t, &b, I2C_MASTER_ACK);
   i2c_master_read_byte (t, &h, I2C_MASTER_ACK);
   i2c_master_read_byte (t, &l, I2C_MASTER_LAST_NACK);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
   {
      ESP_LOGE (TAG, "I2C %02X %04X fail %s", addr & 0x7F, a, esp_err_to_name (err));
      return -1;
   }
   if (s != 4 || b != 2)
   {
      ESP_LOGE (TAG, "I2C %02X %04X %02X %02X %02X%02X Bad", addr & 0x7F, a, s, b, h, l);
      return -1;
   }
   ESP_LOGD (TAG, "I2C %02X %04X %02X %02X %02X%02X OK", addr & 0x7F, a, s, b, h, l);
   return (h << 8) + l;
}

static uint8_t
scd41_crc (uint8_t b1, uint8_t b2)
{
   uint8_t crc = 0xFF;
   void b (uint8_t v)
   {
      crc ^= v;
      uint8_t n = 8;
      while (n--)
      {
         if (crc & 0x80)
            crc = (crc << 1) ^ 0x31;
         else
            crc <<= 1;
      }
   }
   b (b1);
   b (b2);
   return crc;
}

static esp_err_t
scd41_command (uint16_t c)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (scd41i2c << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, c >> 8, true);
   i2c_master_write_byte (t, c, false);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 100 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
      ESP_LOGE (TAG, "I2C %02X %04X fail %s", scd41i2c & 0x7F, c, esp_err_to_name (err));
   return err;
}

static esp_err_t
scd41_read (uint16_t c, int8_t len, uint8_t * buf)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (scd41i2c << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, c >> 8, true);
   i2c_master_write_byte (t, c, true);
   i2c_master_start (t);
   i2c_master_write_byte (t, (scd41i2c << 1) + I2C_MASTER_READ, true);
   i2c_master_read (t, buf, len, I2C_MASTER_LAST_NACK);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 100 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
      ESP_LOGE (TAG, "I2C read %02X %04X %d fail %s", scd41i2c & 0x7F, c, len, esp_err_to_name (err));
   if (!err)
   {                            // CRC checks
      int p = 0;
      while (p < len && !err)
      {
         if (scd41_crc (buf[p], buf[p + 1]) != buf[p + 2])
         {
            ESP_LOGE (TAG, "SCD41 CRC fail on read %02X", c);
            err = ESP_FAIL;
         }
         p += 3;
      }
   }
   return err;
}

static esp_err_t
scd41_write (uint16_t c, uint16_t v)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (scd41i2c << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, c >> 8, true);
   i2c_master_write_byte (t, c, true);
   i2c_master_write_byte (t, v >> 8, true);
   i2c_master_write_byte (t, v, true);
   i2c_master_write_byte (t, scd41_crc (v >> 8, v), true);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 100 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
      ESP_LOGE (TAG, "SCD41 write %02X %04X %04X fail %s", scd41i2c & 0x7F, c, v, esp_err_to_name (err));
   return err;
}

esp_err_t
sht40_write (uint8_t cmd)
{
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (sht40i2c << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte (t, cmd, true);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 100 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
      ESP_LOGE (TAG, "I2C write %02X %02X fail %s", sht40i2c & 0x7F, cmd, esp_err_to_name (err));
   usleep (cmd >= 0xE0 ? 10000 : 1000);
   return err;
}

esp_err_t
sht40_read (uint16_t * ap, uint16_t * bp)
{
   if (ap)
      *ap = 0;
   if (bp)
      *bp = 0;
   uint8_t buf[6];
   i2c_cmd_handle_t t = i2c_cmd_link_create ();
   i2c_master_start (t);
   i2c_master_write_byte (t, (sht40i2c << 1) + I2C_MASTER_READ, true);
   i2c_master_read (t, buf, sizeof (buf), I2C_MASTER_LAST_NACK);
   i2c_master_stop (t);
   esp_err_t err = i2c_master_cmd_begin (i2cport, t, 100 / portTICK_PERIOD_MS);
   i2c_cmd_link_delete (t);
   if (err)
      ESP_LOGE (TAG, "I2C read %02X fail %s", sht40i2c & 0x7F, esp_err_to_name (err));
   if (!err)
   {                            // CRC checks
      int p = 0;
      while (p < sizeof (buf) && !err)
      {
         if (scd41_crc (buf[p], buf[p + 1]) != buf[p + 2])
         {
            ESP_LOGE (TAG, "SCD41 CRC fail on read");
            err = ESP_FAIL;
         }
         p += 3;
      }
   }
   if (!err)
   {
      if (ap)
         *ap = ((buf[0] << 8) | buf[1]);
      if (bp)
         *bp = ((buf[3] << 8) | buf[4]);
   }
   return err;
}

void
i2c_task (void *x)
{
   void fail (uint8_t addr, const char *e)
   {
      ESP_LOGE (TAG, "I2C fail %02X: %s", addr & 0x7F, e);
      jo_t j = jo_object_alloc ();
      jo_string (j, "error", e);
      jo_int (j, "sda", sda.num);
      jo_int (j, "scl", scl.num);
      if (addr)
         jo_stringf (j, "addr", "%02X", addr & 0x7F);
      revk_error ("I2C", &j);
   }
   if (i2c_driver_install (i2cport, I2C_MODE_MASTER, 0, 0, 0))
   {
      fail (0, "Install fail");
      i2cport = -1;
   } else
   {
      i2c_config_t config = {
         .mode = I2C_MODE_MASTER,
         .sda_io_num = sda.num,
         .scl_io_num = scl.num,
         .sda_pullup_en = true,
         .scl_pullup_en = true,
         .master.clk_speed = 100000,
      };
      if (i2c_param_config (i2cport, &config))
      {
         i2c_driver_delete (i2cport);
         fail (0, "Config fail");
         i2cport = -1;
      } else
         i2c_set_timeout (i2cport, 31);
   }
   if (i2cport < 0)
      vTaskDelete (NULL);
   sleep (1);
   // Init
   if (veml6040i2c)
   {
      usleep (10000);
      if (i2c_read_16lh2 (veml6040i2c, 0) < 0)
         fail (veml6040i2c, "VEML6040");
      else
      {
         veml6040 = jo_object_alloc ();
         i2c_write_16lh (veml6040i2c, 0x00, 0x0040);    // IT=4 TRIG=0 AF=0 SD=0
      }
   }
   if (mcp9808i2c)
   {
      usleep (10000);
      if (i2c_read_16hl (mcp9808i2c, 6) != 0x54 || i2c_read_16hl (mcp9808i2c, 7) != 0x0400)
         fail (mcp9808i2c, "MCP9808");
      else
         mcp9808 = jo_object_alloc ();
   }
   if (gzp6816di2c)
   {
      usleep (10000);
      uint8_t v = 0;
      i2c_cmd_handle_t t = i2c_cmd_link_create ();
      i2c_master_start (t);
      i2c_master_write_byte (t, (gzp6816di2c << 1) | I2C_MASTER_READ, true);
      i2c_master_read_byte (t, &v, I2C_MASTER_LAST_NACK);
      i2c_master_stop (t);
      esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
      i2c_cmd_link_delete (t);
      if (err)
         fail (mcp9808i2c, "GZP6816D");
      else
         gzp6816d = jo_object_alloc ();
   }
   if (t6793i2c)
   {
      usleep (10000);
      if (i2c_modbus_read (t6793i2c, 0x1389) < 0)
         fail (t6793i2c, "T6793");
      else
         t6793 = jo_object_alloc ();
   }
   if (scd41i2c)
   {
      usleep (10000);
      esp_err_t err = 0;
      uint8_t try = 5;
      while (try--)
      {
         err = scd41_command (0x3F86);  // Stop periodic
         if (!err)
         {
            usleep (500000);
            break;
         }
         sleep (1);
      }
      uint8_t buf[9];
      if (!err)
      {
         scd41to = ((uint32_t) (scd41dt < 0 ? -scd41dt : 0)) * 65536 / scd41dt_scale / 175;     // Temp offset
         if (!err)
            err = scd41_read (0x2318, 3, buf);  // get offset
         if (!err && scd41to != (buf[0] << 8) + buf[1])
         {
            ESP_LOGE (TAG, "SCD41 TO %u for DT %.2f", scd41to, (float) scd41dt / scd41dt_scale);
            err = scd41_write (0x241D, scd41to);        // set offset
            if (!err)
               err = scd41_command (0x3615);    // persist
            if (!err)
               usleep (800000);
         }
      }
      if (!err)
         err = scd41_read (0x3682, 9, buf);     // Get serial
      if (err)
         fail (scd41i2c, "SCD41");
      else
      {
         scd41_serial =
            ((unsigned long long) buf[0] << 40) + ((unsigned long long) buf[1] << 32) +
            ((unsigned long long) buf[3] << 24) + ((unsigned long long) buf[4] << 16) +
            ((unsigned long long) buf[6] << 8) + ((unsigned long long) buf[7]);
         if (!scd41_command (0x21B1))   // Start periodic
         {
            scd41 = jo_object_alloc ();
            jo_litf (scd41, "serial", "%u", scd41_serial);
         }
      }
   }
   if (tmp1075i2c)
   {
      usleep (10000);
      if (i2c_read_16hl (tmp1075i2c, 0x0F) != 0x7500 || i2c_write_16hl (tmp1075i2c, 1, 0x60FF))
         fail (tmp1075i2c, "TMP1075");
      else
         tmp1075 = jo_object_alloc ();
   }
   if (sht40i2c)
   {
      usleep (10000);
      uint16_t a,
        b;
      usleep (1000);
      if (sht40_write (0x94) || sht40_write (0x89) || sht40_read (&a, &b))
         fail (sht40i2c, "SHT40");
      else
      {
         sht40_serial = (a << 16) + b;
         sht40 = jo_object_alloc ();
         jo_litf (sht40, "serial", "%u", sht40_serial);
      }
   }
   b.ha = 1;
   sleep (5);
   // Poll
   while (!b.die)
   {
      if (gzp6816d)
      {
         i2c_cmd_handle_t t = i2c_cmd_link_create ();
         i2c_master_start (t);
         i2c_master_write_byte (t, (gzp6816di2c << 1) | I2C_MASTER_WRITE, true);
         i2c_master_write_byte (t, 0xAC, false);
         i2c_master_stop (t);
         i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
         i2c_cmd_link_delete (t);
      }
      usleep (500000);
      if (veml6040)
      {                         // Scale to lux
         float w;
         jo_t j = jo_object_alloc ();
         jo_litf (j, "R", "%.2f", (float) i2c_read_16lh2 (veml6040i2c, 0x08) * 1031 / 65535);
         jo_litf (j, "G", "%.2f", (float) i2c_read_16lh2 (veml6040i2c, 0x09) * 1031 / 65535);
         jo_litf (j, "B", "%.2f", (float) i2c_read_16lh2 (veml6040i2c, 0x0A) * 1031 / 65535);
         jo_litf (j, "W", "%.2f", w = (float) i2c_read_16lh2 (veml6040i2c, 0x0B) * 1031 / 65535);
         json_store (&veml6040, j);
         if (veml6040dark && gfxbl.set)
         {
            uint8_t l = (!override && w < (float) veml6040dark / veml6040dark_scale ? 0 : 1);
            if (l != b.bl)
            {
               b.bl = l;
               bl = (l ? gfxhigh : gfxlow);
            }
         }
      }
      if (mcp9808)
      {
         static int16_t last1 = 0,
            last2 = 0,
            last3 = 0;
         int16_t t = (i2c_read_16hl (mcp9808i2c, 5) << 3),
            a = (last1 + last2 + last3 + t) / 4;
         last3 = last2;
         last2 = last1;
         last1 = t;
         jo_t j = jo_object_alloc ();
         jo_litf (j, "C", "%.2f", (float) a / 128 + (float) mcp9808dt / mcp9808dt_scale);
         json_store (&mcp9808, j);
      }
      uint16_t hpa = 0;
      if (gzp6816d)
      {
         uint8_t s,
           p1,
           p2,
           p3,
           t1,
           t2;
         i2c_cmd_handle_t t = i2c_cmd_link_create ();
         i2c_master_start (t);
         i2c_master_write_byte (t, (gzp6816di2c << 1) | I2C_MASTER_READ, true);
         i2c_master_read_byte (t, &s, I2C_MASTER_ACK);
         i2c_master_read_byte (t, &p1, I2C_MASTER_ACK);
         i2c_master_read_byte (t, &p2, I2C_MASTER_ACK);
         i2c_master_read_byte (t, &p3, I2C_MASTER_ACK);
         i2c_master_read_byte (t, &t1, I2C_MASTER_ACK);
         i2c_master_read_byte (t, &t2, I2C_MASTER_LAST_NACK);
         i2c_master_stop (t);
         esp_err_t err = i2c_master_cmd_begin (i2cport, t, 10 / portTICK_PERIOD_MS);
         i2c_cmd_link_delete (t);
         if (!err && !(s & 0x20))
         {
            jo_t j = jo_object_alloc ();
            jo_litf (j, "C", "%.2f", (float) ((t1 << 8) | t2) * 190 / 65536 - 40 + (float) gzp6816ddt / gzp6816ddt_scale);
            float p = (float) 800 * (((p1 << 16) | (p2 << 8) | p3) - 1677722) / 13421772 + 300;
            jo_litf (j, "hPa", "%.3f", p);
            json_store (&gzp6816d, j);
            hpa = p;
         }
      }
      if (t6793)
      {
         int32_t v = i2c_modbus_read (t6793i2c, 0x138B);
         if (v > 0)
         {
            jo_t j = jo_object_alloc ();
            jo_litf (j, "ppm", "%ld", v);
            json_store (&t6793, j);
         }
      }
      if (scd41)
      {
         uint8_t buf[9];
         if (!scd41_read (0xE4B8, 3, buf) && ((buf[0] & 0x7) || buf[1]) && !scd41_read (0xEC05, sizeof (buf), buf))
         {
            if (scd41_crc (buf[0], buf[1]) == buf[2] && scd41_crc (buf[3], buf[4]) == buf[5]
                && scd41_crc (buf[6], buf[7]) == buf[8])
            {
               jo_t j = jo_object_alloc ();
               jo_litf (j, "serial", "%u", scd41_serial);
               jo_litf (j, "ppm", "%u", (buf[0] << 8) + buf[1]);
               if (uptime () >= scd41startup)
               {
                  jo_litf (j, "C", "%.2f",
                           -45.0 + 175.0 * (float) (((uint32_t) ((buf[3] << 8) + buf[4])) + scd41to) / 65536.0 +
                           (float) scd41dt / scd41dt_scale);
                  jo_litf (j, "RH", "%.2f", 100.0 * (float) ((buf[6] << 8) + buf[7]) / 65536.0);
               }
               json_store (&scd41, j);
               if (hpa)
                  scd41_write (0x0E000, hpa);
            } else
               ESP_LOGE (TAG, "SCD41 bad CRC");
         }
      }
      if (tmp1075)
      {
         int32_t v = i2c_read_16hl (tmp1075i2c, 0);
         if (v >= 0)
         {
            jo_t j = jo_object_alloc ();
            jo_litf (j, "C", "%.2f", (float) ((int16_t) v) / 256 + (float) tmp1075dt / tmp1075dt_scale);
            json_store (&tmp1075, j);
         }
      }
      if (sht40)
      {
         uint16_t a,
           b;
         if (!sht40_write (0xFD) && !sht40_read (&a, &b))
         {
            jo_t j = jo_object_alloc ();
            jo_litf (j, "serial", "%u", sht40_serial);
            jo_litf (j, "C", "%.2f", -45.0 + 175.0 * a / 65535.0 + (float) sht40dt / sht40dt_scale);
            jo_litf (j, "RH", "%.2f", -6.0 + 125.0 * b / 65535.0);
            json_store (&sht40, j);
         }
      }
      {                         // Next second
         struct timeval tv;
         gettimeofday (&tv, NULL);
         usleep (1000000 - tv.tv_usec);
      }
   }
   if (scd41)
      scd41_command (0x3F86);   // Stop periodic
   vTaskDelete (NULL);
}

void
i2s_task (void *x)
{
   const int rate = 25;
   const int samples = 320;
   i2s_chan_handle_t mic_handle = { 0 };
   esp_err_t err;
   i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG (I2S_NUM_AUTO, I2S_ROLE_MASTER);
   err = i2s_new_channel (&chan_cfg, NULL, &mic_handle);
   i2s_pdm_rx_config_t cfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG ((samples * rate)),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG (I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
                   .clk = i2sclock.num,
                   .din = i2sdata.num,
                   .invert_flags = {
                                    .clk_inv = i2sclock.invert,
                                    }
                   }
   };
   cfg.slot_cfg.slot_mask = (i2sright ? I2S_PDM_SLOT_RIGHT : I2S_PDM_SLOT_LEFT);
   if (!err)
      err = i2s_channel_init_pdm_rx_mode (mic_handle, &cfg);
   gpio_pulldown_en (i2sdata.num);
   if (!err)
      err = i2s_channel_enable (mic_handle);
   if (err)
   {
      ESP_LOGE (TAG, "Mic I2S failed");
      jo_t j = jo_object_alloc ();
      jo_int (j, "code", err);
      jo_string (j, "error", esp_err_to_name (err));
      jo_int (j, "data", i2sdata.num);
      jo_int (j, "clock", i2sclock.num);
      revk_error ("i2s", &j);
      vTaskDelete (NULL);
      return;
   }
   ESP_LOGE (TAG, "Mic init PDM CLK %d DAT %d %s", i2sclock.num, i2sdata.num, i2sright ? "R" : "L");
   double means[60] = { 0 };
   double peaks[60] = { 0 };
   uint8_t sec = 0;
   int16_t sample[samples];
   uint8_t tick = 0;
   double peak = -INFINITY;
   double sum = 0;
   while (!b.die)
   {
      size_t n = 0;
      err = i2s_channel_read (mic_handle, sample, sizeof (sample), &n, 100);
      if (err || n != sizeof (sample))
      {
         ESP_LOGE (TAG, "Bad read %d %s", n, esp_err_to_name (err));
         sleep (1);
         continue;
      }
      int32_t b = 0;
      for (int i = 0; i < samples; i++)
         b += sample[i];        // DC
      b /= samples;
      uint64_t t = 0;
      for (int i = 0; i < samples; i++)
      {
         int32_t v = (int32_t) sample[i] - b;
         t += v * v;
      }
      double db = log10 ((double) t / samples) * 10 + (double) i2sdb / i2sdb_scale;     // RMS
      if (db > peak)
         peak = db;
      sum += db;
      if (++tick == rate)
      {
         sum /= rate;
         means[sec] = sum;
         peaks[sec] = peak;
         if (++sec == 60)
            sec = 0;
         //ESP_LOGE (TAG, "Peak %.2lf Mean %.2lf", peak, sum);
         jo_t j = jo_object_alloc ();
         if (isfinite (peak))
            jo_litf (j, "peak1", "%.2lf", peak);
         if (isfinite (sum))
            jo_litf (j, "mean1", "%.2lf", sum);
         double p = -INFINITY,
            m = 0;
         for (int s = 50; s < 60; s++)
         {
            if (peaks[(s + sec) % 60] > p)
               p = peaks[(s + sec) % 60];
            m += means[(s + sec) % 60];
         }
         if (isfinite (p))
            jo_litf (j, "peak10", "%.2lf", p);
         if (isfinite (m))
            jo_litf (j, "mean10", "%.2lf", m / 10);
         for (int s = 0; s < 50; s++)
         {
            if (peaks[(s + sec) % 60] > p)
               p = peaks[(s + sec) % 60];
            m += means[(s + sec) % 60];
         }
         if (isfinite (p))
            jo_litf (j, "peak60", "%.2lf", p);
         if (isfinite (m))
            jo_litf (j, "mean60", "%.2lf", m / 60);
         json_store (&noise, j);
         tick = 0;
         peak = -INFINITY;
         sum = 0;
      }
   }
   i2s_channel_disable (mic_handle);
   i2s_del_channel (mic_handle);
   vTaskDelete (NULL);
}

void
ble_task (void *x)
{
   bleenv_run ();
   while (!b.die)
   {
      {                         // Next second
         jo_t j = jo_create_alloc ();
         jo_array (j, NULL);
         for (int i = 0; i < sizeof (blesensor) / sizeof (*blesensor); i++)
         {
            jo_object (j, NULL);
            if (*blesensor[i])
            {
               bleenv_t *e;
               for (e = bleenv; e; e = e->next)
                  if (!strcmp (e->name, blesensor[i]) || !strcmp (e->mac, blesensor[i]))
                     break;
               if (e)
               {
                  if (e->changed)
                  {             // name update
                     e->changed = 0;
                     b.ha = 1;
                  }
                  jo_string (j, "mac", e->mac);
                  if (strcmp (e->mac, e->name))
                     jo_string (j, "name", e->name);
                  if (e->tempset)
                     jo_litf (j, "C", "%.2f", e->temp / 100.0);
                  if (e->humset)
                     jo_litf (j, "RH", "%.2f", e->hum / 100.0);
                  if (e->batset)
                     jo_int (j, "bat", e->bat);
                  if (e->voltset)
                     jo_litf (j, "V", "%.3f", e->volt / 1000.0);
                  if (e->co2set)
                     jo_int (j, "ppm", e->co2);
               } else
                  jo_string (j, "name", blesensor[i]);
            }
            jo_close (j);
         }
         jo_close (j);
         json_store (&bles, j);
         struct timeval tv;
         gettimeofday (&tv, NULL);
         usleep (1000000 - tv.tv_usec);
      }
   }
   vTaskDelete (NULL);
}

void
ds18b20_task (void *x)
{
   ds18b20_device_handle_t adr_ds18b20[10];
   uint64_t id[sizeof (adr_ds18b20) / sizeof (*adr_ds18b20)];
   onewire_bus_config_t bus_config = { ds18b20.num,.flags = {0}
   };
   onewire_bus_rmt_config_t rmt_config = { 20 };
   onewire_bus_handle_t bus_handle = { 0 };
   REVK_ERR_CHECK (onewire_new_bus_rmt (&bus_config, &rmt_config, &bus_handle));
   void init (void)
   {
      onewire_device_iter_handle_t iter = { 0 };
      REVK_ERR_CHECK (onewire_new_device_iter (bus_handle, &iter));
      onewire_device_t dev = { };
      while (!onewire_device_iter_get_next (iter, &dev) && ds18b20_num < sizeof (adr_ds18b20) / sizeof (*adr_ds18b20))
      {
         id[ds18b20_num] = dev.address;
         ds18b20_config_t config = { };
         REVK_ERR_CHECK (ds18b20_new_device (&dev, &config, &adr_ds18b20[ds18b20_num]));
         REVK_ERR_CHECK (ds18b20_set_resolution (adr_ds18b20[ds18b20_num], DS18B20_RESOLUTION_12B));
         ds18b20_num++;
      }
   }
   init ();
   if (!ds18b20_num)
   {
      usleep (100000);
      init ();
   }
   if (!ds18b20_num)
   {
      jo_t j = jo_object_alloc ();
      jo_string (j, "error", "No DS18B20 devices");
      jo_int (j, "port", ds18b20.num);
      revk_error ("temp", &j);
      ESP_LOGE (TAG, "No DS18B20 port %d", ds18b20.num);
      vTaskDelete (NULL);
      return;
   }
   b.ha = 1;
   while (!b.die)
   {
      jo_t j = jo_create_alloc ();
      jo_array (j, NULL);
      float c[ds18b20_num];
      for (int i = 0; i < ds18b20_num; ++i)
      {
         REVK_ERR_CHECK (ds18b20_trigger_temperature_conversion (adr_ds18b20[i]));
         REVK_ERR_CHECK (ds18b20_get_temperature (adr_ds18b20[i], &c[i]));
         jo_object (j, NULL);
         jo_stringf (j, "serial", "%016llX", id[i]);
         if (isfinite (c[i]) && c[i] > -100 && c[i] < 200)
            jo_litf (j, "C", "%.2f", c[i]);
         jo_close (j);
      }
      json_store (&ds18b20s, j);
      {                         // Next second
         struct timeval tv;
         gettimeofday (&tv, NULL);
         usleep (1000000 - tv.tv_usec);
      }
   }
   vTaskDelete (NULL);
}

void
solar_task (void *x)
{
   int backup = 1;
   while (!b.die)
   {
      // Connect
      struct addrinfo base = {.ai_family = PF_UNSPEC,.ai_socktype = SOCK_STREAM };
      struct addrinfo *a = 0,
         *t;
      if (getaddrinfo (solarip, "1502", &base, &a) || !a)
      {
         ESP_LOGE ("Solar", "Cannot look up %s", solarip);
         sleep (10);
         continue;
      }
      int s = -1;
      for (t = a; t; t = t->ai_next)
      {
         s = socket (t->ai_family, t->ai_socktype, t->ai_protocol);
         if (s >= 0 && connect (s, t->ai_addr, t->ai_addrlen))
         {                      // failed to connect
            perror (t->ai_canonname);
            close (s);
            s = -1;
         }
         if (s >= 0)
            break;
      }
      freeaddrinfo (a);
      if (s < 0)
      {
         ESP_LOGE ("Solar", "Cannot connect %s", solarip);
         if (backup < 3600)
            backup *= 2;
         sleep (backup);
         continue;
      }
      backup = 1;

      const char *er = NULL;
      const char *modbus_get (uint16_t reg, uint8_t regs, void *buf)
      {
         if (er)
            return er;
         static uint16_t tag;
         tag++;
         uint8_t req[12];
         req[0] = tag >> 8;     // ID
         req[1] = tag;
         req[2] = req[3] = 0;   // Protocol 0
         req[4] = 0;
         req[5] = 6;            // Len
         req[6] = 1;            // Slave address
         req[7] = 3;            // read multiple holding registers
         req[8] = reg >> 8;
         req[9] = reg;
         req[10] = regs >> 8;
         req[11] = regs;
         int l;
         if ((l = write (s, req, sizeof (req))) != sizeof (req))
            return er = "Bad tx - socket closed?";
         if ((l = read (s, req, 6)) != 6)
            return er = "Bad rx - socket closed?";
         if ((req[0] << 8) + req[1] != tag)
            return er = "Bad tag";
         if (!er && (req[2] || req[3]))
            return er = "Bad protocol";
         uint16_t len = (req[4] << 8) + req[5];
         if (len != regs * 2 + 3)
            return er = "Bad len";
         if ((l = read (s, req + 6, 3)) != 3)
            return er = "Bad rx - socket closed?";
         if ((l = read (s, buf, len - 3)) != len - 3)
            return er = "Bad rx - socket closed?";
         if (req[7] != 3)
            return er = "Bad function";
         return NULL;
      }
      void modbus_string (uint16_t reg, uint8_t len, char *buf)
      {                         // Last byte will have null
         *buf = 0;
         modbus_get (reg, len / 2, buf);
         buf[len - 1] = 0;
      }

      int16_t modbus_16d (uint16_t reg)
      {
         uint8_t buf[2];
         modbus_get (reg, 1, buf);
         return (buf[0] << 8) + buf[1];
      }

      uint16_t modbus_16 (uint16_t reg)
      {
         uint8_t buf[2];
         modbus_get (reg, 1, buf);
         return (buf[0] << 8) + buf[1];
      }

      uint32_t modbus_32 (uint16_t reg)
      {
         uint8_t buf[4];
         modbus_get (reg, 2, buf);
         return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
      }

      // Base data

      char manufacturer[32 + 1];
      modbus_string (0x9c44, sizeof (manufacturer), manufacturer);
      char model[32 + 1];
      modbus_string (0x9c54, sizeof (model), model);
      char version[16 + 1];
      modbus_string (0x9c6c, sizeof (version), version);
      char serial[32 + 1];
      modbus_string (0x9c74, sizeof (serial), serial);

      int16_t voltage_scale = modbus_16d (0x9c92);
      int16_t frequency_scale = modbus_16d (0x9c96);
      int16_t total_scale = modbus_16d (0x9c9f);

      while (!er && !b.die)
      {
         jo_t j = jo_object_alloc ();
         jo_string (j, "manufacturer", manufacturer);
         jo_string (j, "model", model);
         jo_string (j, "version", version);
         jo_string (j, "serial", serial);

         void addvalue (const char *tag, int32_t val, int16_t scale)
         {
            if (scale >= 0)
            {
               uint32_t s = 1;
               while (scale--)
                  s *= 10;
               jo_int (j, tag, val * s);
               return;
            }
            const char *sign = "";
            if (val < 0)
            {
               val = -val;
               sign = "-";
            }
            int d = -scale;
            uint32_t s = 1;
            while (scale++)
               s *= 10;
            jo_litf (j, tag, "%s%lu.%0*lu", sign, val / s, d, val % s);
         }

         // We assumed scale is fixed, it is not.
         int16_t power_scale = modbus_16d (0x9c94) - 3; // kW instead of W

         addvalue ("voltage", modbus_16 (0x9c8c), voltage_scale);
         addvalue ("frequency", modbus_16 (0x9c95), frequency_scale);
         addvalue ("power", modbus_16 (0x9c93), power_scale);
         uint32_t total = modbus_32 (0x9c9d);

         time_t now = time (0);
         struct tm t;
         localtime_r (&now, &t);
         uint32_t day = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
         if (day != solarday)
         {
            jo_t s = jo_object_alloc ();
            jo_int (s, "solarday", day);
            jo_int (s, "solarsod", total);
            revk_setting (s);
            jo_free (&s);
         }
         addvalue ("today", total - solarsod, total_scale - 3); // kWh
         json_store (&solar, j);
         sleep (10);
      }
      if (er)
         ESP_LOGE ("Solar", "Solar failed %s", er);
      close (s);
   }
   vTaskDelete (NULL);
}

static void
ir_callback (uint8_t coding, uint16_t lead0, uint16_t lead1, uint8_t len, uint8_t * data)
{                               // Handle generic IR https://www.amazon.co.uk/dp/B07DJ58XGC
   //ESP_LOGE (TAG, "IR CB %d %d %d %d", coding, lead0, lead1, len);
   static uint32_t key = 0;
   static uint8_t count = 0;
   if (coding == IR_PDC && len == 32 && lead0 > 8500 && lead0 < 9500 && lead1 > 4000 && lead1 < 5000 && (data[2] ^ data[3]) == 0xFF)
   {                            // Key (NEC) - normally address and inverted address, but allow for special cases, as long as code and inverted code.
      key = (((data[0] ^ ~data[1]) & 0xFF) << 16 | (data[0] << 8) | data[2]);
      count = 1;
      //ESP_LOGE (TAG, "Key %04lX", key);
   }
   if (count && coding == IR_ZERO && len == 1 && lead0 > 8500 && lead0 < 9500 && lead1 > 1500 && lead1 < 2500 && key)
   {                            // Continue - ignore for now
      if (count < 255)
         count++;
      if (count == 10)
      {                         // hold
         jo_t j = jo_create_alloc ();
         jo_stringf (j, NULL, "%04lX", key);
         revk_info ("irhold", &j);
      }
   }
   if (count && coding == IR_IDLE)
   {
      jo_t j = jo_create_alloc ();
      jo_stringf (j, NULL, "%04lX", key);
      if (count < 10)
         revk_info ("irpress", &j);
      else
         revk_info ("irrelease", &j);
      count = 0;
   }
}

char *
dollar_diff (time_t ref, time_t now, const char *colon)
{
   char *c = NULL;
   time_t diff = 0;
   if (ref > now)
   {                            // Count up
      time_t s = now;
      now = ref;
      ref = s;
   }
   if (ref && now)
      diff = now - ref;
   if (colon && *colon && strchr (colon, '%'))
   {                            // time formatted
      struct tm nowt;
      struct tm reft;
      gmtime_r (&now, &nowt);
      gmtime_r (&ref, &reft);
      struct tm t = { 0 };
      // Work out manual offset as a datetime
      t.tm_sec = diff % 60;
      t.tm_min = (diff / 60) % 60;
      t.tm_hour = (ref / 3600) % 24;
      // Date diff
      t.tm_year = nowt.tm_year - reft.tm_year;
      t.tm_mon = nowt.tm_mon - reft.tm_mon;
      t.tm_mday = nowt.tm_mday - reft.tm_mday;
      t.tm_yday = nowt.tm_yday - reft.tm_yday;
      if (t.tm_mday < 0)
      {
         t.tm_mday += 30;       // ??
         t.tm_mon--;
      }
      if (t.tm_mon < 0)
      {
         t.tm_mon += 12;
         t.tm_year--;
         t.tm_yday += 365;      // ?
      }
      if (!strstr (colon, "%y") && !strstr (colon, "%F"))
      {                         // No year
         if (!strstr (colon, "%m") && !strstr (colon, "%F"))
         {                      // No month
            if (!strstr (colon, "%d") && !strstr (colon, "%e") && !strstr (colon, "%F"))
            {                   // No day
               if (!strstr (colon, "%H") && !strstr (colon, "%T"))
               {                // No hour
                  if (!strstr (colon, "%M") && !strstr (colon, "%T"))
                  {             // No minute
                     t.tm_sec = diff;
                  } else
                     t.tm_min = diff / 60;
               } else
                  t.tm_hour = diff / 86400 * 24;
            } else
               t.tm_mday = diff / 86400;
         } else
            t.tm_mon += t.tm_year * 12;
      }
      t.tm_year -= 1900;        // base year 0000
      t.tm_mon--;               // 1 gets added

      char temp[100];
      *temp = 0;
      strftime (temp, sizeof (temp), colon, &t);
      return strdup (temp);
   }
   if (!diff)
      c = strdup ("--:--");
   else if (diff < 86400)
      asprintf (&c, "%02lld:%02lld", diff / 3600, diff / 60 % 60);
   else if (diff < 864000)
      asprintf (&c, "%lld.%03lld", diff / 86400, diff * 10 / 864 % 1000);
   else if (diff < 8640000)
      asprintf (&c, "%lld.%02lld", diff / 86400, diff / 864 % 100);
   else if (diff < 86400000)
      asprintf (&c, "%lld.%01lld", diff / 86400, diff / 8640 % 10);
   else if (ref < 864000000)
      asprintf (&c, "%lld", diff / 86400);
   else
      c = strdup ("----");
   return c;
}

char *
dollar_time (time_t now, const char *fmt)
{
   struct tm t;
   int l = strlen (fmt);
   if (l >= +5 && (fmt[l - 5] == '-' || fmt[l - 5] == '+') && isdigit ((int) (uint8_t) fmt[l - 4])
       && isdigit ((int) (uint8_t) fmt[l - 3]) && isdigit ((int) (uint8_t) fmt[l - 2]) && isdigit ((int) (uint8_t) fmt[l - 1]))
   {
      uint32_t a = ((fmt[l - 4] - '0') * 10 + fmt[l - 3] - '0') * 3600 + ((fmt[l - 2] - '0') * 10 + fmt[l - 1] - '0') * 60;
      if (fmt[l - 5] == '-')
         now -= a;
      else
         now += a;
      fmt = (const char *) strdupa (fmt);
      ((char *) fmt)[l - 5] = 0;
      gmtime_r (&now, &t);
   } else if (l >= 1 && fmt[l - 1] == 'Z')
   {
      fmt = (const char *) strdupa (fmt);
      ((char *) fmt)[l - 1] = 0;
      gmtime_r (&now, &t);
   } else
      localtime_r (&now, &t);
   if (!*fmt)
      fmt = hhmm;
   char temp[100];
   *temp = 0;
   strftime (temp, sizeof (temp), fmt, &t);
   return strdup (temp);
}

char *
suffix_number (long double v, uint8_t places, const char *tag)
{
   int target = INT_MIN;
   int margin = 0;
   int digits = 0;
   char *r = NULL;
   char zero = 0;
   while (*tag)
   {
      if (*tag >= '0' && *tag <= '9')
      {
         int d = 0;
         zero = 0;
         if (*tag == '0')
            zero = 1;
         while (*tag >= '0' && *tag <= '9')
            d = d * 10 + *tag++ - '0';
         if (*tag == '.')
         {
            digits = d;
            tag++;
            if (*tag >= '0' && *tag <= '9')
            {
               d = 0;
               while (*tag >= '0' && *tag <= '9')
                  d = d * 10 + *tag++ - '0';
               places = d;
            }
         } else
            places = d;         // just places not total digits
         tag--;
      } else if (*tag == 'd')
         v /= 10;
      else if (*tag == 'c')
         v /= 100;
      else if (*tag == 'k')
         v /= 1000;
      else if (*tag == 'M')
         v /= 1000000;
      else if (*tag == 'm')
         v *= 1000;
      else if (*tag == 'u')
         v *= 1000000;
      else if (*tag == 'D')
         v *= 10;
      else if (*tag == 'K')
         v += 273.15;
      else if (*tag == 'F')
         v = (v + 40) * 1.8 - 40;
      else if (*tag == 'C')
         v = (v + 40) / 1.8 - 40;
      else if (*tag == '=')
      {                         // target number, no decimal
         tag++;
         target = 0;
         int8_t s = 1;
         if (*tag == '-')
         {
            s = -1;
            tag++;
         }
         while (*tag >= '0' && *tag <= '9')
            target = target * 10 + *tag++ - '0';
         target *= s;
         continue;
      } else if (*tag == 0xC2 && tag[1] == 0xB1)        // ±
      {                         // margin for target
         tag += 2;
         margin = 0;
         while (*tag >= '0' && *tag <= '9')
            margin = margin * 10 + *tag++ - '0';
         continue;
      }
      tag++;
   }
   if (zero)
      asprintf (&r, "%0*.*Lf", digits, places, v);
   else if (digits)
      asprintf (&r, "%*.*Lf", digits, places, v);
   else
      asprintf (&r, "%.*Lf", places, v);
   if (target != INT_MIN)
   {                            // Colour override if out of range
      if (!margin)
         margin = 5;            // default
      if (v < target - margin)
         gfx_foreground (0x0000FF);
      else if (v > target + margin)
         gfx_foreground (0xFF0000);
   }
   return r;
}

char *
dollar_json (jo_t * jp, const char *dot, const char *colon)
{
   if (!jp || !dot)
      return NULL;
   xSemaphoreTake (json_mutex, portMAX_DELAY);
   char *res = NULL;
   jo_t j = *jp;
   if (j)
   {
      jo_type_t t = jo_find (j, dot);
      if (t)
      {
         res = jo_strdup (j);
         if (res && *res && colon && *colon && t == JO_NUMBER)
         {
            uint8_t places = 0;
            char *d = strchr (res, '.');
            if (d)
               places = strlen (d + 1);
            free (res);
            res = suffix_number (jo_read_float (j), places, colon);
         }
      }
   }
   xSemaphoreGive (json_mutex);
   return res;
}

char *
dollar (const char *c, const char *dot, const char *colon, time_t now)
{                               // Single dollar replace - c is the part after $. Return is NULL or malloc result
   struct tm t;
   localtime_r (&now, &t);
   char *r = NULL;
   if (!strcasecmp (c, "SEASON"))
      return strndup (revk_season (time (0)), 1);
   if (!strcasecmp (c, "SEASONS"))
      return strdup (revk_season (time (0)));
   if (!strcasecmp (c, "TIME"))
      return dollar_time (now, colon ? : hhmm);
   if (!strcasecmp (c, "DATE"))
      return dollar_time (now, colon ? : "%F");
   if (!strcasecmp (c, "DAY"))
      return strdup (longday[t.tm_wday]);
   if (!strcasecmp (c, "COUNTDOWN"))
   {
      time_t ref = parse_time (refdate, t.tm_year + 1900);
      if (ref < now)
         ref = parse_time (refdate, t.tm_year + 1901);  // Counting up, allow for year being next year
      return dollar_diff (ref, now, colon);
   }
   if (!strcasecmp (c, "ID"))
      return strdup (revk_id);
   if (!strcasecmp (c, "HOSTNAME"))
      return strdup (hostname);
   if (!strcasecmp (c, "SSID"))
      return strdup (*qrssid ? qrssid : wifissid);
   if (!strcasecmp (c, "PASS"))
      return strdup (*qrssid ? qrpass : wifipass);
   if (!strcasecmp (c, "WIFI"))
   {
      if (*qrssid ? *qrpass : *wifipass)
         asprintf (&r, "WIFI:S:%s;T:WPA2;P:%s;;", *qrssid ? qrssid : wifissid, *qrssid ? qrpass : wifipass);
      else
         asprintf (&r, "WIFI:S:%s;;", *qrssid ? qrssid : wifissid);
      return r;
   }
   if (!strcasecmp (c, "IPV4"))
   {
      char ip[16];
      revk_ipv4 (ip);
      return strdup (ip);
   }
   if (!strcasecmp (c, "IPV6") || !strcasecmp (c, "IP"))
   {
      char ip[40];
      revk_ipv6 (ip);
      return strdup (ip);
   }
#ifdef	CONFIG_REVK_SOLAR
   if (!strcasecmp (c, "SUNSET") && now && (poslat || poslon))
      return dollar_time (sunset, colon ? : hhmm);
   if (!strcasecmp (c, "SUNRISE") && now && (poslat || poslon))
      return dollar_time (sunrise, colon ? : hhmm);
#endif
   if (!strcasecmp (c, "FULLMOON"))
      return dollar_time (revk_moon_full_next (now), colon ? : "%F %H:%M");
   if (!strcasecmp (c, "NEWMOON"))
      return dollar_time (revk_moon_new (now), colon ? : "%F %H:%M");
   if (!strncasecmp (c, "MOONPHASE", 9))
   {
      int n = atoi (c + 9) ? : 360;
      asprintf (&r, "%d", (revk_moon_phase (now) * n + 180) / 360 % n);
      return r;
   }
   if (!strcasecmp (c, "DEFCON"))
   {
      if (b.defcon > 5)
         return strdup ("-");
      asprintf (&r, "%u", b.defcon);
      return r;
   }
   if (!strcasecmp (c, "API"))
      return dollar_json (&apijson, dot, colon);
   if (!strcasecmp (c, "NOISE"))
      return dollar_json (&noise, dot, colon);
   if (!strcasecmp (c, "WEATHER"))
   {
      r = NULL;
      xSemaphoreTake (json_mutex, portMAX_DELAY);
      if (colon && !strcmp (colon, "128") && jo_find (weather, dot))
      {                         // Fudge weather
         char *i = jo_strdup (weather);
         if (i)
         {
            char *s = strstr (i, "64x64");
            asprintf (&r, "%.*s128x128%s", (int) (s - i), i, s + 5);
            free (i);
         }
      }
      xSemaphoreGive (json_mutex);
      if (!r)
         r = dollar_json (&weather, dot, colon);
      return r;
   }
   if (!strcasecmp (c, "SOLAR"))
      return dollar_json (&solar, dot, colon);
   if (!strcasecmp (c, "TEMPERATURE"))
   {
      if (ds18b20s)
         return dollar_json (&ds18b20s, dot ? : "[0].C", colon);
      if (bles)
         return dollar_json (&bles, dot ? : "[0].C", colon);
      if (scd41)
         return dollar_json (&scd41, dot ? : "C", colon);
      if (tmp1075)
         return dollar_json (&tmp1075, dot ? : "C", colon);
      if (mcp9808)
         return dollar_json (&mcp9808, dot ? : "C", colon);
   }
   if (!strcasecmp (c, "CO2") || !strcasecmp (c, "CO₂"))
   {
      if (scd41)
         return dollar_json (&scd41, dot ? : "ppm", colon);
      if (t6793)
         return dollar_json (&t6793, dot ? : "ppm", colon);
   }
   if (!strcasecmp (c, "HUMIDITY"))
   {
      if (scd41)
         return dollar_json (&scd41, dot ? : "RH", colon);
   }
   if (!strncasecmp (c, "DS18B20", 7))  // allow for [0] directly with no dot
      return dollar_json (&ds18b20s, dot ? : "[0].C", colon);
   if (!strncasecmp (c, "BLE", 3))      // allow for [0] directly with no dot
      return dollar_json (&bles, dot ? : "[0].C", colon);
   if (tmp1075 && !strcasecmp (c, "TMP1075"))
      return dollar_json (&tmp1075, dot ? : "C", colon);
   if (mcp9808 && !strcasecmp (c, "MCP9808"))
      return dollar_json (&mcp9808, dot ? : "C", colon);
   if (gzp6816d && (!strcasecmp (c, "GZP6816D") || !strcasecmp (c, "PRESSURE")))
      return dollar_json (&gzp6816d, dot ? : "hPa", colon);
   if (scd41 && !strcasecmp (c, "SCD41"))
      return dollar_json (&scd41, dot ? : "ppm", colon);
   if (sht40 && !strcasecmp (c, "SHT40"))
      return dollar_json (&sht40, dot ? : "C", colon);
   if (t6793 && !strcasecmp (c, "T6793"))
      return dollar_json (&t6793, dot ? : "ppm", colon);
   if (veml6040 && (!strcasecmp (c, "VEML6040") || !strcasecmp (c, "LIGHT")))
      return dollar_json (&veml6040, dot ? : "W", colon);
   if (noise && !strcasecmp (c, "NOISE"))
      return dollar_json (&noise, dot ? : "mean1", colon);
   if (!strncasecmp (c, "MQTT", 4) && isdigit ((int) (uint8_t) c[4]) && !c[5] && c[4] > '0'
       && c[4] <= '0' + sizeof (mqttjson) / sizeof (*mqttjson))
      return dollar_json (&mqttjson[c[4] - '1'], dot, colon);
   if (!strcasecmp (c, "SNMPHOST") && snmp.host)
      return strdup (snmp.host);
   if (!strcasecmp (c, "SNMPDESC") && snmp.desc)
      return strdup (snmp.desc);
   if (!strcasecmp (c, "SNMPFBVER") && snmp.desc)
   {
      char *s = snmp.desc;
      while (*s && *s != '(')
         s++;
      if (*s)
      {
         s++;
         char *e = s;
         while (*e && *e != ' ' && *e != ')')
            e++;
         return strndup (s, e - s);
      }
   }
   if (!strcasecmp (c, "SNMPUPTIME"))
      return dollar_diff (snmp.upfrom, now, colon);
   return r;
}

const char *
dollar_check (const char *c, const char *tag)
{                               // Check if dollar tag present - allow any suffix
   while ((c = strchr (c, '$')))
   {
      c++;
      if (*c == '$')
         c++;
      else
      {
         if (*c == '{')
         {
            c++;
            if (*c == '$')
               c++;
            if (!strncasecmp (c, tag, strlen (tag)))
               return c;
            while (*c && *c != '}')
               c++;
            continue;
         }
         if (!strncasecmp (c, tag, strlen (tag)))
            return c;
      }
   }
   return NULL;
}

char *
dollars (char *c, time_t now)
{                               // Check c for $ expansion, return c, or malloc'd replacement
   if (!strchr (c, '$'))
      return c;
   char nested = 0;
   char *old = c;
   char *new = NULL;
   size_t len;
   FILE *o = open_memstream (&new, &len);
   while (*c)
   {
      char *d = strchr (c, '$');
      if (d)
      {                         // Expand
         if (d > c)
            fwrite (c, d - c, 1, o);
         d++;
         if (*d == '$')
         {                      // $$ is $
            fputc ('$', o);
            c = d + 1;
         } else if (*d != '{' && !isalpha ((int) (uint8_t) * d))
         {                      // not sensible escape
            fputc ('$', o);
            c = d;
         } else
         {
            char *x = NULL;
            if (*d == '{')
            {
               d++;
               if (*d == '$')
               {
                  nested = 1;
                  d++;
               }
               char *e = strchr (d, '}');
               if (!e)
                  break;
               x = strndup (d, e - d);
               c = e + 1;
            } else
            {
               char *e = d;
               while (*e && (isalnum ((int) (uint8_t) * e) || *e == '_'))
                  e++;
               x = strndup (d, e - d);
               c = e;
            }
            if (x)
            {
               char *dot = x,
                  *colon = NULL;
               while (*dot && isalnum ((int) (uint8_t) * dot))
                  dot++;
               if (*dot == ':')
               {
                  colon = dot;
                  *colon++ = 0;
                  dot = NULL;
               } else if (*dot == '.' || *dot == '[')
               {
                  if (*dot == '.')
                     *dot++ = 0;
                  colon = dot;
                  while (*colon && *colon != ':')
                     colon++;
                  if (*colon)
                     *colon++ = 0;
                  else
                     colon = NULL;
               } else
                  dot = NULL;
               char *n = dollar (x, dot, colon, now);
               ESP_LOGD (TAG, "Expand [%s.%s:%s] [%s]", x, dot ? : "", colon ? : "", n ? : "-");
               free (x);
               if (n)
                  fprintf (o, "%s", n);
               free (n);
            }

         }
      } else
      {                         // Copy
         fprintf (o, "%s", c);
         break;
      }
   }
   fclose (o);
   if (nested)
   {
      char *new2 = dollars (new, now);
      if (new2 != new)
      {
         if (new != old)
            free (new);
         new = new2;
      }
   }
   return new;
}

void
mqttjson_cb (void *arg, const char *topic, jo_t j)
{
   uint32_t i = (int) arg;
   if (i >= sizeof (jsonsub) / sizeof (*jsonsub))
      return;
   json_store (&mqttjson[i], jo_dup (j));
}

void
reload_task (void *x)
{
   while (1)
   {
      file_t *i;
      for (i = files; i; i = i->next)
         if (i->reload)
            download (i->url, i->suffix, 1, i->cachetime);
      sleep (1);
   }
}

void
btn_task (void *x)
{
   btng[0] = btnu;
   btng[1] = btnd;
   btng[2] = btnl;
   btng[3] = btnr;
   btng[4] = btnp;
   // We accept one button at a time
   uint8_t b;
   for (b = 0; b < sizeof (btns) / sizeof (*btns); b++)
      revk_gpio_input (btng[b]);
   while (1)
   {
      // Wait for a button press
      for (b = 0; b < sizeof (btns) / sizeof (*btns) && !revk_gpio_get (btng[b]); b++);
      if (b == sizeof (btns) / sizeof (*btns))
      {
         usleep (10000);
         continue;
      }
      uint8_t c = 0;
      while (revk_gpio_get (btng[b]))
      {
         if (c < 255)
            c++;
         if (c == LONG_PRESS)
         {
            jo_t j = jo_create_alloc ();
            jo_string (j, NULL, "long");
            revk_info (btns[b], &j);
         }
         usleep (10000);
      }
      if (c >= 5)
      {
         jo_t j = jo_create_alloc ();
         jo_string (j, NULL, c < LONG_PRESS ? "short" : "release");
         revk_info (btns[b], &j);
      }
      // Wait all clear
      c = 0;
      while (1)
      {
         for (b = 0; b < sizeof (btns) / sizeof (*btns) && !revk_gpio_get (btng[b]); b++);
         if (b < sizeof (btns) / sizeof (*btns))
            c = 0;
         if (++c == 5)
            break;
         usleep (10000);
      }
   }
}

void
api_get (void)
{
   if (!*apiurl)
      return;
   file_t *w = download (apiurl, NULL, 0, cacheapi);
   if (!w || !w->updated)
      return;
   w->updated = 0;
   xSemaphoreTake (file_mutex, portMAX_DELAY);
   if (w && w->data && w->json)
   {
      jo_t j = jo_parse_mem (w->data, w->size);
      if (j)
         json_store (&apijson, jo_dup (j));
      jo_free (&j);
   }
   xSemaphoreGive (file_mutex);
}

void
weather_get (void)
{
   if ((!poslat && !poslon && !*postown) || !*weatherapi || revk_link_down ())
      return;
   char *url;
   if (*postown)
      asprintf (&url, "http://api.weatherapi.com/v1/forecast.json?key=%s&q=%s", weatherapi, postown);
   else
      asprintf (&url, "http://api.weatherapi.com/v1/forecast.json?key=%s&q=%f,%f", weatherapi,
                (float) poslat / 10000000, (float) poslon / 1000000);
   file_t *w = download (url, NULL, 0, cacheweather);
   free (url);
   if (!w || !w->updated)
      return;
   w->updated = 0;
   xSemaphoreTake (file_mutex, portMAX_DELAY);
   if (w && w->data && w->json)
   {
      jo_t j = jo_parse_mem (w->data, w->size);
      if (j)
         json_store (&weather, jo_dup (j));
      jo_free (&j);
   }
   xSemaphoreGive (file_mutex);
}

void
ha_config (void)
{
   b.ha = 0;
   if (haannounce)
   {
      ha_config_sensor ("ram",.name = "RAM",.field = "mem",.unit = "B");
      ha_config_sensor ("spi",.name = "PSRAM",.field = "spi",.unit = "B");
      ha_config_sensor ("veml6040W",.name = "VEML6040-White",.type = "illuminance",.unit = "lx",.field =
                        "veml6040.W",.delete = !veml6040);
      ha_config_sensor ("veml6040R",.name = "VEML6040-Red",.type = "illuminance",.unit = "lx",.field = "veml6040.R",.delete =
                        !veml6040);
      ha_config_sensor ("veml6040G",.name = "VEML6040-Green",.type = "illuminance",.unit = "lx",.field =
                        "veml6040.G",.delete = !veml6040);
      ha_config_sensor ("veml6040B",.name = "VEML6040-Blue",.type = "illuminance",.unit = "lx",.field =
                        "veml6040.B",.delete = !veml6040);
      ha_config_sensor ("mcp9808T",.name = "MCP9808",.type = "temperature",.unit = "°C",.field = "mcp9808.C",.delete = !mcp9808);
      ha_config_sensor ("tmp1075T",.name = "TMP1075",.type = "temperature",.unit = "°C",.field = "tmp1075.C",.delete = !tmp1075);
      ha_config_sensor ("noiseM1",.name = "NOISE-MEAN1",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.mean1",.delete = !noise || reporting > 1);
      ha_config_sensor ("noiseP1",.name = "NOISE-PEAK1",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.peak1",.delete = !noise || reporting > 1);
      ha_config_sensor ("noiseM10",.name = "NOISE-MEAN10",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.mean10",.delete = !noise || reporting > 10);
      ha_config_sensor ("noiseP10",.name = "NOISE-PEAK10",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.peak10",.delete = !noise || reporting > 10);
      ha_config_sensor ("noiseM60",.name = "NOISE-MEAN60",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.mean60",.delete = !noise);
      ha_config_sensor ("noiseP60",.name = "NOISE-PEAK60",.type = "sound_pressure",.unit = "dB",.field =
                        "noise.peak60",.delete = !noise);
      for (int i = 0; i < ds18b20_num; i++)
      {
         char id[20],
           name[20],
           js[20];
         sprintf (id, "ds18b20%dT", i);
         sprintf (name, "DS18B20-%d", i);
         sprintf (js, "ds18b20[%d].C", i);
         ha_config_sensor (id,.name = name,.type = "temperature",.unit = "°C",.field = js);
      }
      for (int i = 0; i < sizeof (blesensor) / sizeof (*blesensor); i++)
      {
         bleenv_t *e = NULL;
         if (*blesensor[i])
            for (e = bleenv; e; e = e->next)
               if (!strcmp (e->mac, blesensor[i]) || !strcmp (e->name, blesensor[i]))
                  break;
         char id[20],
           name[50],
           js[20];
         sprintf (id, "ble%dT", i);
         if (e)
            sprintf (name, "BLE-Temp-%s", e->name);
         else
            sprintf (name, "BLE-Temp-%d", i + 1);
         sprintf (js, "ble[%d].C", i);
         ha_config_sensor (id,.name = name,.type = "temperature",.unit = "°C",.field = js,.delete = *blesensor[i] ? 0 : 1);
         sprintf (id, "ble%dR", i);
         if (e)
            sprintf (name, "BLE-RH-%s", e->name);
         else
            sprintf (name, "BLE-RH-%d", i + 1);
         sprintf (js, "ble[%d].RH", i);
         ha_config_sensor (id,.name = name,.type = "humidity",.unit = "%",.field = js,.delete = *blesensor[i] ? 0 : 1);
         sprintf (id, "ble%dB", i);
         if (e)
            sprintf (name, "BLE-Bat-%s", e->name);
         else
            sprintf (name, "BLE-Bat-%d", i + 1);
         sprintf (js, "ble[%d].bat", i);
         ha_config_sensor (id,.name = name,.type = "battery",.unit = "%",.field = js,.delete = *blesensor[i] ? 0 : 1);
      }
      ha_config_sensor ("gzp6816dP",.name = "GZP6816D-Pressure",.type = "pressure",.unit = "mbar",.field =
                        "gzp6816d.hPa",.delete = !gzp6816d);
      ha_config_sensor ("gzp6816dT",.name = "GZP6816D-Temp",.type = "temperature",.unit = "°C",.field = "gzp6816d.C",.delete =
                        !gzp6816d);
      ha_config_sensor ("scd41C",.name = "SCD41-CO₂",.type = "carbon_dioxide",.unit = "ppm",.field = "scd41.ppm",.delete =
                        !scd41);
      ha_config_sensor ("scd41T",.name = "SCD41-Temp",.type = "temperature",.unit = "°C",.field = "scd41.C",.delete = !scd41);
      ha_config_sensor ("scd41H",.name = "SCD41-Humidity",.type = "humidity",.unit = "%",.field = "scd41.RH",.delete = !scd41);
      ha_config_sensor ("sht40T",.name = "SHT40-Temp",.type = "temperature",.unit = "°C",.field = "sht40.C",.delete = !sht40);
      ha_config_sensor ("sht40H",.name = "SHT40-Humidity",.type = "humidity",.unit = "%",.field = "sht40.RH",.delete = !sht40);
      ha_config_sensor ("t6793C",.name = "T6793-CO₂",.type = "carbon_dioxide",.unit = "ppm",.field = "t6793.ppm",.delete =
                        !t6793);
      ha_config_sensor ("solarV",.name = "Solar-Voltage",.type = "voltage",.unit = "V",.field = "solar.voltage",.delete = !solar);
      ha_config_sensor ("solarF",.name = "Solar-Frequency",.type = "frequency",.unit = "Hz",.field =
                        "solar.frequency",.delete = !solar);
      ha_config_sensor ("solarP",.name = "Solar-Power",.type = "power",.unit = "W",.field = "solar.power",.delete = !solar);
      for (int b = 0; b < sizeof (btns) / sizeof (*btns); b++)
      {
         char t[10];
         sprintf (t, "/%s", btns[b]);
         char st[10];
         sprintf (st, "button_%d", b + 1);
         char n[10];
         sprintf (n, "S%s", btns[b]);
         ha_config_trigger (n,.info = t,.subtype = btns[b],.payload = "short",.delete = !btng[b].set);
         sprintf (n, "L%s", btns[b]);
         ha_config_trigger (n,.info = t,.subtype = btns[b],.payload = "long",.type = "button_long_press",.delete = !btng[b].set);
         sprintf (n, "R%s", btns[b]);
         ha_config_trigger (n,.info = t,.subtype = btns[b],.payload = "release",.type = "button_long_release",.delete =
                            !btng[b].set);
      }
   }
   if (hairkeys && irgpio.set)
   {
      const char irkeys[] = "123456789*0#ULPRD";
      const char ircode[] = "45464744404307150916190D18081C5A52";
      for (int i = 0; irkeys[i]; i++)
      {
         char k[] = { irkeys[i], 0 };
         char p[] = { '0', '0', ircode[i * 2], ircode[i * 2 + 1], 0 };
         char n[] = { 'S', '0', '0', ircode[i * 2], ircode[i * 2 + 1], 0 };
         ha_config_trigger (n,.info = "/irpress",.subtype = k,.payload = p);
      }
   }
}

void
app_main ()
{
   ESP_LOGE (TAG, "Started");
   b.defcon = 7;
   snmp.sock = -1;
   revk_boot (&app_callback);
   revk_start ();
   epd_mutex = xSemaphoreCreateMutex ();
   xSemaphoreGive (epd_mutex);
   json_mutex = xSemaphoreCreateMutex ();
   xSemaphoreGive (json_mutex);
   file_mutex = xSemaphoreCreateMutex ();
   xSemaphoreGive (file_mutex);
   revk_mqtt_sub (0, "DEFCON/#", defcon_cb, NULL);
   for (int i = 0; i < sizeof (jsonsub) / sizeof (*jsonsub); i++)
      if (*jsonsub[i])
         revk_mqtt_sub (0, jsonsub[i], mqttjson_cb, (void *) i);
   if (sda.set && scl.set)
      revk_task ("i2c", i2c_task, NULL, 4);
   if (i2sdata.set && i2sclock.set)
      revk_task ("i2s", i2s_task, NULL, 10);
   if (ds18b20.set)
      revk_task ("18b20", ds18b20_task, NULL, 4);
   if (irgpio.set)
      ir_start (irgpio, ir_callback);
   if (lightcount && lightgpio.set)
   {
      led_strip_config_t strip_config = {
         .strip_gpio_num = (lightgpio.num),
         .max_leds = lightcount,
         .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
         .led_model = LED_MODEL_WS2812, // LED strip model
         .flags.invert_out = lightgpio.invert,  // whether to invert the output signal(useful when your hardware has a level inverter)
      };
      led_strip_rmt_config_t rmt_config = {
         .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
         .resolution_hz = 10 * 1000 * 1000,     // 10 MHz
         .flags.with_dma = true,
      };
      REVK_ERR_CHECK (led_strip_new_rmt_device (&strip_config, &rmt_config, &strip));
      showlights ("b");
      revk_task ("blink", led_task, NULL, 4);
   }
   // Web interface
   httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
   config.stack_size += 1024 * 4;
   config.lru_purge_enable = true;
   config.max_uri_handlers = 3 + revk_num_web_handlers ();
   if (!httpd_start (&webserver, &config))
   {
      register_get_uri ("/", web_root);
      register_get_uri ("/status", web_status);
#ifdef	CONFIG_LWPNG_ENCODE
      register_get_uri ("/frame.png", web_frame);
#endif
      revk_web_settings_add (webserver);
   }
   if (btnu.set || btnd.set || btnl.set || btnr.set)
      revk_task ("btn", btn_task, NULL, 4);
#ifndef	CONFIG_GFX_BUILD_SUFFIX_GFXNONE
   if (gfxbl.set)
      revk_task ("BL", bl_task, NULL, 4);
   {
    const char *e = gfx_init (pwr: gfxpwr.num, ena: gfxena.num, cs: gfxcs.num, sck: gfxsck.num, mosi: gfxmosi.num, dc: gfxdc.num, rst: gfxrst.num, busy: gfxbusy.num, flip: gfxflip, direct: 1, invert:gfxinvert);
      if (e)
      {
         ESP_LOGE (TAG, "gfx %s", e);
         jo_t j = jo_object_alloc ();
         jo_string (j, "error", "Failed to start");
         jo_string (j, "description", e);
         revk_error ("gfx", &j);
      }
      bl = gfxhigh;
      xSemaphoreTake (epd_mutex, portMAX_DELAY);
      revk_gfx_init (startup);
      xSemaphoreGive (epd_mutex);
   }
#endif
   revk_task ("snmp", snmp_rx_task, NULL, 4);
   if (*solarip)
      revk_task ("solar", solar_task, NULL, 10);
   revk_task ("reload", reload_task, NULL, 10);
   if (gfxena.set)
   {
      gpio_reset_pin (gfxena.num);
      gpio_set_direction (gfxena.num, GPIO_MODE_OUTPUT);
      gpio_set_level (gfxena.num, gfxena.invert);       // Enable
   }
   if (bleenable)
      revk_task ("ble", ble_task, NULL, 4);
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
#ifdef	GFX_EPD
   void flash (void)
   {                            // Random data
      uint32_t r = 0;
      epd_lock ();
      for (int y = 0; y < gfx_height (); y++)
         for (int x = 0; x < gfx_width (); x++)
         {
            if (!(x & 31))
               r = esp_random ();
            gfx_pixel (x, y, (r & 1) ? 255 : 0);
            r >>= 1;
         }
      gfx_refresh ();
      epd_unlock ();
   }
#endif
   showlights ("");
   int16_t lastday = -1;
   int8_t lasthour = -1;
   int8_t lastmin = -1;
   int8_t lastsec = -1;
   int8_t lastreport = -1;
   while (!revk_shutting_down (NULL))
   {
      usleep (10000);
      if (b.ha)
         ha_config ();
      time_t now = time (0);
#ifdef	GFX_EPD
      now += 2;                 // Slow update
#endif
      if (now < 1000000000)
         now = 0;
      uint32_t up = uptime ();
      struct tm t;
      localtime_r (&now, &t);
#ifdef	TIMINGS
      uint64_t timea = esp_timer_get_time ();
#endif
      if (reporting && (int8_t) (now / reporting) != lastreport)
      {
         lastreport = now / reporting;
         revk_command ("status", NULL);
      }
      if (t.tm_yday != lastday)
      {                         // Daily
         lastday = t.tm_yday;
         lasthour = -1;
#ifdef	CONFIG_REVK_SOLAR
         struct tm tm;
         sunrise =
            sun_rise (t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, (double) poslat / poslat_scale, (double) poslon / poslon_scale,
                      SUN_DEFAULT);
         localtime_r (&sunrise, &tm);
         sunrisehhmm = tm.tm_hour * 100 + t.tm_min;
         sunset =
            sun_set (t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, (double) poslat / poslat_scale, (double) poslon / poslon_scale,
                     SUN_DEFAULT);
         localtime_r (&sunset, &tm);
         sunsethhmm = tm.tm_hour * 100 + t.tm_min;
#endif
      }
      if (t.tm_hour != lasthour)
      {                         // Hourly
         lasthour = t.tm_hour;
         lastmin = -1;
      }
      if (t.tm_min != lastmin)
      {                         // Per minute
         lastmin = t.tm_min;
         lastsec = -1;
#ifndef	ESP_EPD
         b.redraw = 1;
#endif
      }
      if (t.tm_sec != lastsec)
      {                         // Per sec
#ifdef	GFX_EPD
         lastsec = t.tm_sec;
#endif
         weather_get ();
         api_get ();
      }
      if (b.setting)
      {
         b.setting = 0;
         b.redraw = 1;
      }
      if (b.wificonnect)
      {
         snmp_tx ();
         weather_get ();
         api_get ();
         gfx_refresh ();
         b.redraw = 1;
         b.startup = 1;
         b.wificonnect = 0;
      }
      if (overrideimage && gfxmessage)
      {
         char *was = (void *) overrideimage;
         overrideimage = NULL;
         if (was)
         {
            file_t *i = download (was, ".png", 0, cachepng);
            if (i && i->w)
            {
               epd_lock ();
               gfx_clear (0);
               gfx_refresh ();
               plot (i, gfx_width () / 2 - i->w / 2, gfx_height () / 2 - i->h / 2,
#ifdef	GFX_EPD
                     gfxinvert
#else
                     0
#endif
                  );
               epd_unlock ();
               override = up + gfxmessage;
            }
            free (was);
         }
      } else if (overridemessage && gfxmessage)
      {
         char *m = (void *) overridemessage;
         overridemessage = NULL;
         gfx_message (m);
         free (m);
         override = up + gfxmessage;
      }
      if (override)
      {
         if (override < up)
         {
            override = 0;
            b.redraw = 1;
         } else
            continue;
      }
#ifdef	GFX_EPD
      if (!b.startup || !b.redraw)
         continue;
#else
      if (!b.startup || (t.tm_sec == lastsec && !b.redraw))
         continue;
      lastsec = t.tm_sec;
#endif
      season = *revk_season (now);
      if (*seasoncode)
         season = *seasoncode;
      if (*lightpattern && !b.lightoverride)
      {
         uint16_t on = lighton,
            off = lightoff;
#ifdef	CONFIG_REVK_SOLAR
         uint16_t ss (uint16_t x)
         {                      // sun based adjust
            int16_t d = 0;
            if (x > 4000 && x < 6000)
            {
               d = x - 5000;
               x = sunrisehhmm;
            } else if (x > 6000 && x < 8000)
            {
               d = x - 7000;
               x = sunsethhmm;
            }
            if (d)
            {
               x = (x / 100) * 60 + (x % 100);
               x += d;
               x = (x / 60) * 100 + (x % 60);
            }
            return x;
         }
         on = ss (on);
         off = ss (off);
#endif
         int hhmm = t.tm_hour * 100 + t.tm_min;
         showlights (on == off || (on < off && on <= hhmm && off > hhmm)
                     || (off < on && (on <= hhmm || off > hhmm)) ? lightpattern : "");
      }
      b.redraw = 0;
      // Image
#ifdef	GFX_EPD
      if (gfxnight && t.tm_hour >= 2 && t.tm_hour < 4)
      {
         flash ();
         gfx_refresh ();        // Full update
         b.redraw = 1;
      }
      static uint32_t fresh = 0;
      if (refresh && (!gfxnight || refresh < 86400) && now / refresh != fresh)
      {                         // Or, periodic refresh, e.g. once a day or more
         fresh = now / refresh;
         gfx_refresh ();
         b.redraw = 1;
      }
#endif
#ifndef	CONFIG_GFX_BUILD_SUFFIX_GFXNONE
      epd_lock ();
#ifdef	TIMINGS
      uint64_t timeb = esp_timer_get_time ();
#endif
      gfx_clear (0);
      uint8_t h = 0,
         v = 0;
      for (int w = 0; w < WIDGETS; w++)
      {
         gfx_align_t a = 0;
         if (widgeth[w] != REVK_SETTINGS_WIDGETH_PREV)
            h = widgeth[w];
         if (widgetv[w] != REVK_SETTINGS_WIDGETV_PREV)
            v = widgetv[w];
         if (h <= REVK_SETTINGS_WIDGETH_CENTRE)
            a |= GFX_L;
         if (h >= REVK_SETTINGS_WIDGETH_CENTRE)
            a |= GFX_R;
         if (v <= REVK_SETTINGS_WIDGETV_MIDDLE)
            a |= GFX_T;
         if (v >= REVK_SETTINGS_WIDGETV_MIDDLE)
            a |= GFX_B;
         if (w + 1 < WIDGETS && widgeth[w + 1] == REVK_SETTINGS_WIDGETH_PREV)
            a |= GFX_H;
         if (w + 1 < WIDGETS && widgetv[w + 1] == REVK_SETTINGS_WIDGETV_PREV)
            a |= GFX_V;
         gfx_pos_t x = widgetx[w];
         gfx_pos_t y = widgety[w];
         if (widgeth[w] == REVK_SETTINGS_WIDGETH_PREV || widgetv[w] == REVK_SETTINGS_WIDGETV_PREV)
            gfx_pos (gfx_x () + x, gfx_y () + y, a);    // Relative
         else
         {                      // Absolute
            if (x < 0)
               x += gfx_width ();
            if (y < 0)
               y += gfx_height ();
            gfx_pos (x, y, a);
         }
#ifdef	GFX_COLOUR
         gfx_background (gfx_rgb (*widgetb[w] ? : *background));
         gfx_foreground (*widgetf[w] ? gfx_rgb (*widgetf[w]) : contrast (gfx_b ()));
#else
         gfx_foreground (widgetk[w] == REVK_SETTINGS_WIDGETK_NORMAL || widgetk[w] == REVK_SETTINGS_WIDGETK_MASK ? 0 : 0xFFFFFF);
         gfx_background (widgetk[w] == REVK_SETTINGS_WIDGETK_NORMAL
                         || widgetk[w] == REVK_SETTINGS_WIDGETK_MASKINVERT ? 0xFFFFFF : 0);
#endif
         // Content substitutions
         char *ca = strdup (widgetc[w]);
         char *c = dollars (ca, now);
         switch (widgett[w])
         {
         case REVK_SETTINGS_WIDGETT_TEXT:
            if (*c)
            {
               uint16_t s = widgets[w];
               uint8_t flags = 0;
               if (s & 0x8000)
                  flags |= GFX_TEXT_DESCENDERS;
               if (s & 0x4000)
                  flags |= GFX_TEXT_LIGHT;
               if (s & 0x2000)
                  flags |= GFX_TEXT_ITALIC;
               s &= 0xFFF;
               if (!s)
                  s = 5;
               gfx_text (flags, s, "%s", c);
            }
            break;
         case REVK_SETTINGS_WIDGETT_BLOCKS:
            if (*c)
            {
               uint16_t s = widgets[w];
               uint8_t flags = 0;
               if (s & 0x8000)
                  flags |= GFX_TEXT_DESCENDERS;
               if (s & 0x4000)
                  flags |= GFX_TEXT_LIGHT;
               if (s & 0x2000)
                  flags |= GFX_TEXT_DOTTY;
               else
                  flags |= GFX_TEXT_BLOCKY;
               s &= 0xFFF;
               if (!s)
                  s = 4;
               gfx_text (flags, s, "%s", c);
            }
            break;
         case REVK_SETTINGS_WIDGETT_DIGITS:
            if (*c)
            {
               gfx_pos_t s = widgets[w];
               uint8_t flags = 0;
               if (s & 0x8000)
                  flags |= GFX_7SEG_SMALL_DOT;
               if (s & 0x4000)
                  flags |= GFX_7SEG_SMALL_COLON;
               if (s & 0x2000)
                  flags |= GFX_7SEG_ITALIC;
               s &= 0xFFF;
               if (!s)
                  s = 4;
               gfx_7seg (flags, s, "%s", c);
            }
            break;
         case REVK_SETTINGS_WIDGETT_IMAGE:
            if (*c)
            {
               file_t *i = NULL;
               char *s = strrchr (c, '*');
               if (s)
               {                // Season logic
                  char *url = strdup (c);
                  s = strrchr (url, '*');
                  if (season)
                  {
                     *s = season;
                     i = download (url, ".png", 0, cachepng);
                  }
                  if (!i || !i->size)
                  {
                     strcpy (s, s + 1);
                     i = download (url, ".png", 0, cachepng);
                  }

               } else
                  i = download (c, ".png", 0, cachepng);
               if (i && i->size && i->w && i->h)
               {
                  gfx_pos_t ox,
                    oy;
                  gfx_draw (i->w, i->h, 0, 0, &ox, &oy);
                  plot (i, ox, oy
#ifndef	GFX_COLOUR
                        , widgetk[w] == REVK_SETTINGS_WIDGETK_MASKINVERT || widgetk[w] == REVK_SETTINGS_WIDGETK_INVERT
#else
                        , 0
#endif
                     );
               }
            }
            break;
         case REVK_SETTINGS_WIDGETT_QR:
            gfx_qr (c, widgets[w]);
            break;
         case REVK_SETTINGS_WIDGETT_HLINE:
            {
               gfx_pos_t ox,
                 oy,
                 s = (widgets[w] & 0xFFF) ? : gfx_width ();;
               gfx_draw (s, 1, 0, 0, &ox, &oy);
               gfx_line (ox, oy, ox + s, oy);
            }
            break;
         case REVK_SETTINGS_WIDGETT_VLINE:
            {
               gfx_pos_t ox,
                 oy,
                 s = (widgets[w] & 0xFFF) ? : gfx_width ();;
               gfx_draw (1, s, 0, 0, &ox, &oy);
               gfx_line (ox, oy, ox, oy + s);
            }
            break;
         case REVK_SETTINGS_WIDGETT_BINS:
            extern void widget_bins (uint16_t, const char *);
            widget_bins (widgets[w], c);
            break;
         case REVK_SETTINGS_WIDGETT_CLOCK:
            extern void widget_clock (uint16_t, const char *);
            widget_clock (widgets[w], c);
            break;
         }
         if (c != ca)
            free (c);
         free (ca);
      }
#ifdef	TIMINGS
      uint64_t timec = esp_timer_get_time ();
#endif
      epd_unlock ();
#endif
#ifdef	TIMINGS
      uint64_t timed = esp_timer_get_time ();
      ESP_LOGE (TAG, "Setup %4lldms, plot %4lldms, update %4lldms", (timeb - timea + 500) / 10000,
                (timec - timeb + 500) / 1000, (timed - timec + 500) / 1000);
#endif
      snmp_tx ();
   }
   b.die = 1;
#ifdef	GFX_LCD
   bl = gfxhigh;
   while (1)
   {
      epd_lock ();
      gfx_clear (0);
      const char *reason;
      int t = revk_shutting_down (&reason);
      if (t < 3)
         bl = 0;
      if (t > 1)
      {
         gfx_text (0, 5, "Reboot");
         gfx_pos (gfx_width () / 2, gfx_height () / 2, GFX_C | GFX_M);
         gfx_text (1, 2, "%s", reason);
         int i = revk_ota_progress ();
         if (i >= 0 && i <= 100)
         {
            gfx_pos (gfx_width () / 2 - 50, gfx_height () - 1, GFX_L | GFX_B | GFX_H);
            gfx_7seg (0, 5, "%3d", i);
            gfx_text (0, 5, "%%", i);
         }
      }
      epd_unlock ();
      usleep (100000);
   }
#else
   bl = gfxlow;
   sleep (1);
#endif
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
#ifdef	GFX_COLOUR
      revk_web_setting (req, "Background", "background");
#else
      revk_web_setting (req, "Image invert", "gfxinvert");
#endif
      if (lightgpio.set && lightcount)
      {
         revk_web_setting_title (req, "LEDs");
         revk_web_setting (req, "Light pattern", "lightpattern");
         revk_web_setting (req, "Light on", "lighton");
         revk_web_setting (req, "Light off", "lightoff");
      }
      if (bleenable)
      {
         revk_web_setting_title (req, "BLE sensors");
         for (int i = 0; i < sizeof (blesensor) / sizeof (*blesensor); i++)
            settings_ble (req, i);
      }
      return;
   }
   revk_web_setting_title (req, "Widget settings %d", page);
   revk_web_setting_info (req, "This build is for a display %dx%d pixels", gfx_width (), gfx_height ());
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_TEXT || widgett[page - 1] == REVK_SETTINGS_WIDGETT_BLOCKS)
      revk_web_setting_info (req, "Font size allows _ for descenders, | for light, / for italics.");
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_DIGITS)
      revk_web_setting_info (req, "Font size allows _ for small after ., | for small after :, / for italics.");
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_QR)
      revk_web_setting_info (req, "Size allows _ for unit size (else overall size), | for no quite zone, / for special.");
   void add (const char *pre, const char *tag)
   {
      char name[20];
      sprintf (name, "%s%d", tag, page);
      revk_web_setting (req, pre, name);
   }
   add (NULL, "tab");
   add (NULL, "widgett");
#ifdef	GFX_COLOUR
   add (NULL, "widgetf");
   add (NULL, "widgetb");
#else
   add (NULL, "widgetk");
#endif
   add (NULL, "widgeth");
   add (NULL, "widgetx");
   add (NULL, "widgetv");
   add (NULL, "widgety");
   const char *c = widgetc[page - 1];
   const char *p = NULL;
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_TEXT || widgett[page - 1] == REVK_SETTINGS_WIDGETT_BLOCKS ||
       widgett[page - 1] == REVK_SETTINGS_WIDGETT_DIGITS || widgett[page - 1] == REVK_SETTINGS_WIDGETT_BINS)
      p = "Font size";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_HLINE)
      p = "Line width";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_VLINE)
      p = "Line height";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_QR)
      p = "Size";
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_IMAGE)
      add (p, "widgets");
   p = NULL;
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE)
      p = "PNG Image URL";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_BINS)
      p = "Bins data JSON URL";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_QR)
      p = "QR code content";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_CLOCK)
      p = "Time";
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_VLINE && widgett[page - 1] != REVK_SETTINGS_WIDGETT_HLINE)
      add (p, "widgetc");
   // Extra fields
   const char *found;
   if ((found = dollar_check (c, "WEATHER")))
   {
      revk_web_setting (req, NULL, "weatherapi");
      revk_web_setting (req, NULL, "postown");
   }
   if ((found = dollar_check (c, "API")))
   {
      revk_web_setting (req, NULL, "apiurl");
      revk_web_setting (req, NULL, "cacheapi");
   }
   if (found || dollar_check (c, "SUN"))
   {
      revk_web_setting (req, NULL, "poslat");
      revk_web_setting (req, NULL, "poslon");
   }
   if ((found = dollar_check (c, "WIFI")) || dollar_check (c, "SSID"))
      revk_web_setting (req, NULL, "qrssid");
   if (found || dollar_check (c, "PASS"))
      revk_web_setting (req, NULL, "qrpass");
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE && strchr (c, '*'))
      revk_web_setting (req, NULL, "seasoncode");
   if (dollar_check (c, "COUNTDOWN"))
      revk_web_setting (req, NULL, "refdate");
   if (dollar_check (c, "SNMP"))
      revk_web_setting (req, NULL, "snmphost");
   const char *m = dollar_check (c, "MQTT");
   if (m)
   {
      char temp[] = "jsonsubx";
      temp[7] = m[4];
      revk_web_setting (req, NULL, temp);
   }
   // Notes
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE)
      revk_web_setting_info (req, "URL should be http://, and can include * for season character");
   else if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_BINS)
      revk_web_setting_info (req,
                             "Content can contain <tt>$</tt> expansion fields like <tt>$TIME</tt>, <tt>${DAY}</tt>. See manual for more details");
}

void
revk_state_extra (jo_t j)
{
   xSemaphoreTake (json_mutex, portMAX_DELAY);
   if (veml6040)
      jo_json (j, "veml6040", veml6040);
   if (mcp9808)
      jo_json (j, "mcp9808", mcp9808);
   if (gzp6816d)
      jo_json (j, "gzp6816d", gzp6816d);
   if (t6793)
      jo_json (j, "t6793", t6793);
   if (scd41)
      jo_json (j, "scd41", scd41);
   if (sht40)
      jo_json (j, "sht40", sht40);
   if (tmp1075)
      jo_json (j, "tmp1075", tmp1075);
   if (ds18b20s)
      jo_json (j, "ds18b20", ds18b20s);
   if (solar)
      jo_json (j, "solar", solar);
   if (bles)
      jo_json (j, "ble", bles);
   if (noise)
      jo_json (j, "noise", noise);
   xSemaphoreGive (json_mutex);
}
