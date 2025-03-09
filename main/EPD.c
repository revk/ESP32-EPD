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
#include "EPD.h"

#define	LEFT	0x80            // Flags on font size
#define	RIGHT	0x40
#define	LINE	0x20
#define	MASK	0x1F
#define MINSIZE	4

const char sd_mount[] = "/sd";
uint64_t sdsize = 0,            // SD card data
   sdfree = 0;

char season = 0;                // Current season

static struct
{                               // Flags
   uint8_t wificonnect:1;
   uint8_t redraw:1;
   uint8_t setting:1;
   uint8_t lightoverride:1;
   uint8_t startup:1;
   uint8_t defcon:3;
} volatile b = { 0 };

volatile uint32_t override = 0;

jo_t weather = NULL;
jo_t json = NULL;
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

httpd_handle_t webserver = NULL;

led_strip_handle_t strip = NULL;
sdmmc_card_t *card = NULL;

static SemaphoreHandle_t epd_mutex = NULL;

const char *
gfx_qr (const char *value, uint32_t max)
{
#ifndef	CONFIG_GFX_NONE
   unsigned int width = 0;
 uint8_t *qr = qr_encode (strlen (value), value, widthp:&width);
   if (!qr)
      return "Failed to encode";
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
   for (int y = 0; y < width; y++)
      for (int x = 0; x < width; x++)
         for (int dy = 0; dy < s; dy++)
            for (int dx = 0; dx < s; dx++)
               gfx_pixel (ox + x * s + dx, oy + y * s + dy, qr[width * y + x] & QR_TAG_BLACK ? 0xFF : 0);
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
   for (int i = 1; i < leds; i++)
   {
      revk_led (strip, i, 255, revk_rgb (*c));
      if (*c)
         c++;
      if (!*c)
         c = rgb;
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

char *
setdefcon (int level, char *value)
{                               // DEFCON state
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
      b.defcon = l;
   } else
      b.defcon = level;
   return "";
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
   if (prefix && !strcmp (prefix, "DEFCON") && target && isdigit ((int) *target) && !target[1])
   {
      const char *err = setdefcon (*target - '0', value);
      b.redraw = 1;
      return err;
   }
   if (client || !prefix || target || strcmp (prefix, topiccommand) || !suffix)
      return NULL;
   // Not for us or not a command from main MQTT
   if (!strcmp (suffix, "json"))
   {
      jo_free (&json);
      json = jo_dup (j);
      b.setting = 1;
   }
   if (!strcmp (suffix, "setting"))
   {
      b.setting = 1;
      snmp.lasttx = 0;          // Force re-lookup
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

file_t *files = NULL;

file_t *
find_file (char *url)
{
   file_t *i;
   for (i = files; i && strcmp (i->url, url); i = i->next);
   if (!i)
   {
      i = mallocspi (sizeof (*i));
      if (i)
      {
         memset (i, 0, sizeof (*i));
         i->url = strdup (url);
         i->next = files;
         files = i;
      }
   }
   return i;
}

void
check_file (file_t * i)
{
   if (!i || !i->data || !i->size)
      return;
   i->changed = time (0);
   const char *e1 = lwpng_get_info (i->size, i->data, &i->w, &i->h);
   if (!e1)
   {
      i->json = 0;              // PNG
      ESP_LOGE (TAG, "Image %s len %lu width %lu height %lu", i->url, i->size, i->w, i->h);
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
         ESP_LOGE (TAG, "JSON %s len %lu", i->url, i->size);
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
}

file_t *
download (char *url, const char *suffix)
{
   file_t *i = find_file (url);
   if (!i)
      return i;
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
   ESP_LOGD (TAG, "Get %s", url);
   int32_t len = 0;
   uint8_t *buf = NULL;
   esp_http_client_config_t config = {
      .url = url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 20000,
   };
   int response = -1;
   if (i->cache > uptime ())
      response = (i->data ? 304 : 404); // Cached
   else if (!revk_link_down () && (!strncasecmp (url, "http://", 7) || !strncasecmp (url, "https://", 8)))
   {
      i->cache = uptime () + cachetime;
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
            if (response != 200 && response != 304)
               ESP_LOGE (TAG, "Bad response %s (%d)", url, response);
            esp_http_client_close (client);
         }
         esp_http_client_cleanup (client);
      }
      ESP_LOGD (TAG, "Got %s %d", url, response);
   }
   if (response != 304)
   {
      if (response != 200)
      {                         // Failed
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
            free (i->data);
            i->data = buf;
            i->size = len;
            check_file (i);
         }
         buf = NULL;
      }
   }
   if (card)
   {                            // SD
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
         while (*q && isalnum ((int) (uint8_t) * q))
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
                        free (i->data);
                        i->data = buf;
                        i->size = s.st_size;
                        check_file (i);
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
   if (a & 0x8000)
      gfx_pixel (p->ox + x, p->oy + y, (r / 3 + g / 3 + b / 3) / 256);
   return NULL;
}

void
plot (file_t * i, gfx_pos_t ox, gfx_pos_t oy)
{
   plot_t settings = { ox, oy };
   lwpng_decode_t *p = lwpng_decode (&settings, NULL, &pixel, &my_alloc, &my_free, NULL);
   lwpng_data (p, i->size, i->data);
   const char *e = lwpng_decoded (&p);
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
web_root (httpd_req_t * req)
{
   if (revk_link_down ())
      return revk_web_settings (req);   // Direct to web set up
   revk_web_head (req, *hostname ? hostname : appname);
#ifdef	CONFIG_LWPNG_ENCODE
   revk_web_send (req, "<p>");
   int32_t w = gfx_width ();
   int32_t h = gfx_height ();
#define	DIV 2
   revk_web_send (req, "<div style='display:inline-block;width:%dpx;height:%dpx;margin:5px;border:10px solid %s;border-%s:20px solid %s;'><img width=%d height=%d src='frame.png' style='transform:",   //
                  w / DIV, h / DIV,     //
                  gfxinvert ? "black" : "white",        //
                  gfxflip & 4 ? gfxflip & 2 ? "left" : "right" : gfxflip & 2 ? "top" : "bottom",        //
                  gfxinvert ? "black" : "white",        //
                  gfx_raw_w () / DIV, gfx_raw_h () / DIV        //
      );
   if (gfxflip & 4)
      revk_web_send (req, "translate(%dpx,%dpx)rotate(90deg)scale(1,-1)",       //
                     (w - h) / 2 / DIV, (h - w) / 2 / DIV);
   revk_web_send (req, "scale(%d,%d);'></div>", gfxflip & 1 ? -1 : 1, gfxflip & 2 ? -1 : 1      //
      );
#undef DIV
   revk_web_send (req, "</p><p><a href=/>Reload</a></p>");
#endif
   return revk_web_foot (req, 0, 1, NULL);
}

void
epd_lock (void)
{
   xSemaphoreTake (epd_mutex, portMAX_DELAY);
   gfx_lock ();
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
   epd_lock ();
   uint8_t *png = NULL;
   size_t len = 0;
   uint32_t w = gfx_raw_w ();
   uint32_t h = gfx_raw_h ();
   uint8_t *b = gfx_raw_b ();
   ESP_LOGD (TAG, "Encode W=%lu H=%lu", w, h);
   lwpng_encode_t *p = lwpng_encode_1bit (w, h, &my_alloc, &my_free, NULL);
   if (b)
      while (h--)
      {
         lwpng_encode_scanline (p, b);
         b += (w + 7) / 8;
      }
   const char *e = lwpng_encoded (&p, &len, &png);
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
   epd_unlock ();
   return ESP_OK;
}
#endif

void
snmp_tx (void)
{                               // Send an SNMP if neede
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
snmp_rx_task (void *x)
{                               // Get SNMP responses
   while (1)
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
}

void
led_task (void *x)
{
   while (1)
   {
      usleep (100000);
      revk_led (strip, 0, 255, revk_blinker ());
      led_strip_refresh (strip);
   }
}

char *
dollar_diff (time_t ref, time_t now)
{
   char *c = NULL;
   if (ref && now)
      ref -= now;
   if (ref < 0)
      ref = 0 - ref;
   if (!ref)
      c = strdup ("--:--");
   else if (ref < 86400)
      asprintf (&c, "%02lld:%02lld", ref / 3600, ref / 60 % 60);
   else if (ref < 864000)
      asprintf (&c, "%lld.%03lld", ref / 86400, ref * 10 / 864 % 1000);
   else if (ref < 8640000)
      asprintf (&c, "%lld.%02lld", ref / 86400, ref / 864 % 100);
   else if (ref < 86400000)
      asprintf (&c, "%lld.%01lld", ref / 86400, ref / 8640 % 10);
   else if (ref < 864000000)
      asprintf (&c, "%lld", ref / 86400);
   else
      c = strdup ("----");
   return c;
}

char *
dollar (char *c, time_t now)
{                               // Return c or a malloc'd expanded c
   if (*c != '$')
      return c;
   struct tm t;
   localtime_r (&now, &t);
   if (!strcmp (c + 1, "TIME"))
      asprintf (&c, "%02d:%02d", t.tm_hour, t.tm_min);
   else if (!strcmp (c + 1, "DATE"))
      asprintf (&c, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
   else if (!strcmp (c + 1, "DAY"))
      c = strdup (longday[t.tm_wday]);
   else if (!strcmp (c + 1, "COUNTDOWN"))
   {
      time_t ref = parse_time (refdate, t.tm_year + 1900);
      if (ref < now)
         ref = parse_time (refdate, t.tm_year + 1901);  // Counting up, allow for year being next year
      c = dollar_diff (ref, now);
   } else if (!strcmp (c + 1, "SSID"))
      c = strdup (*qrssid ? qrssid : wifissid);
   else if (!strcmp (c + 1, "PASS"))
      c = strdup (*qrssid ? qrpass : wifipass);
   else if (!strcmp (c + 1, "WIFI"))
   {
      if (*qrssid ? *qrpass : *wifipass)
         asprintf (&c, "WIFI:S:%s;T:WPA2;P:%s;;", *qrssid ? qrssid : wifissid, *qrssid ? qrpass : wifipass);
      else
         asprintf (&c, "WIFI:S:%s;;", *qrssid ? qrssid : wifissid);
   } else if (!strcmp (c + 1, "IPV4"))
   {
      esp_netif_ip_info_t ip;
      if (!esp_netif_get_ip_info (ap_netif, &ip) && ip.ip.addr)
         asprintf (&c, IPSTR, IP2STR (&ip.ip));
   }
#ifdef CONFIG_LWIP_IPV6
   else if (!strcmp (c + 1, "IPV6") || !strcmp (c + 1, "IP"))
   {
      esp_ip6_addr_t ip[LWIP_IPV6_NUM_ADDRESSES];
      int n = esp_netif_get_all_ip6 (sta_netif, ip);
      if (n)
      {
         for (int i = 0; i < n && i < 4; i++)
            if (n == 1 || ip[i].addr[0] != 0x000080FE)  // Yeh FE80 backwards
            {
               asprintf (&c, IPV6STR, IPV62STR (ip[i]));
               for (char *q = c; *q; q++)
                  *q = toupper (*q);
               break;
            }
      }
   }
#endif
#ifdef	CONFIG_REVK_SOLAR
   else if (!strcmp (c + 1, "SUNSET") && now && (poslat || poslon))
   {
      time_t when = sun_set (t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, (double) poslat / poslat_scale,
                             (double) poslon / poslon_scale, SUN_DEFAULT);
      struct tm tm = { 0 };
      localtime_r (&when, &tm);
      asprintf (&c, "%02d:%02d", tm.tm_hour, tm.tm_min);
   } else if (!strcmp (c + 1, "SUNRISE") && now && (poslat || poslon))
   {
      time_t when = sun_rise (t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, (double) poslat / poslat_scale,
                              (double) poslon / poslon_scale, SUN_DEFAULT);
      struct tm tm = { 0 };
      localtime_r (&when, &tm);
      asprintf (&c, "%02d:%02d", tm.tm_hour, tm.tm_min);
   }
#endif
   else if (!strcmp (c + 1, "FULLMOON"))
   {
      time_t when = revk_moon_full_next (now);
      struct tm tm = { 0 };
      localtime_r (&when, &tm);
      asprintf (&c, "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
   } else if (!strcmp (c + 1, "DEFCON"))
   {
      if (b.defcon > 5)
         c = strdup ("-");
      else
         asprintf (&c, "%u", b.defcon);
   } else if (weather && !strncmp (c + 1, "WEATHER.", 8))
   {                            // Weather data
      if (jo_find (weather, c + 9))
         c = jo_strdup (weather);
   } else if (json && !strncmp (c + 1, "JSON.", 5))
   {                            // Weather data
      if (jo_find (json, c + 6))
         c = jo_strdup (json);
   } else if (!strcmp (c + 1, "SNMPHOST") && snmp.host)
      c = strdup (snmp.host);
   else if (!strcmp (c + 1, "SNMPDESC") && snmp.desc)
      c = strdup (snmp.desc);
   else if (!strcmp (c + 1, "SNMPFBVER") && snmp.desc)
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
         c = strndup (s, e - s);
      }
   } else if (!strcmp (c + 1, "SNMPUPTIME"))
      c = dollar_diff (snmp.upfrom, now);
   return c;
}

void
app_main ()
{
   b.defcon = 7;
   snmp.sock = -1;
   revk_boot (&app_callback);
   revk_start ();
   epd_mutex = xSemaphoreCreateMutex ();
   xSemaphoreGive (epd_mutex);
   // Web interface
   httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
   config.lru_purge_enable = true;
   config.max_uri_handlers = 2 + revk_num_web_handlers ();
   if (!httpd_start (&webserver, &config))
   {
      register_get_uri ("/", web_root);
#ifdef	CONFIG_LWPNG_ENCODE
      if (gfx_bpp () == 1)
         register_get_uri ("/frame.png", web_frame);
#endif
      revk_web_settings_add (webserver);
   }

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
      revk_task ("blink", led_task, NULL, 4);
   }
   revk_task ("snmp", snmp_rx_task, NULL, 4);
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
   if (gfxflash)
   {
      epd_lock ();
      gfx_clear (0);
      for (int y = 0; y < gfx_height (); y++)
         for (int x = (y & 1); x < gfx_width (); x += 2)
            gfx_pixel (x, y, 255);
      gfx_refresh ();
      epd_unlock ();
      epd_lock ();
      gfx_clear (0);
      for (int y = 0; y < gfx_height (); y++)
         for (int x = 1 - (y & 1); x < gfx_width (); x += 2)
            gfx_pixel (x, y, 255);
      gfx_refresh ();
      epd_unlock ();
   }
   showlights ("");
   uint32_t fresh = 0;
   uint32_t min = 0;
   while (1)
   {
      usleep (100000);
      time_t now = time (0);
      if (now < 1000000000)
         now = 0;
      struct tm t;
      localtime_r (&now, &t);
      if (b.setting)
      {
         b.setting = 0;
         b.redraw = 1;
      }
      uint32_t up = uptime ();
      if (b.wificonnect)
      {
         snmp_tx ();
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
               epd_lock ();
               gfx_message (msg);
               int max = gfx_height () - gfx_y ();
               if (max > 0)
               {
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
               }
               epd_unlock ();
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
            continue;
      }
      if (!b.startup || (now / 60 == min && !b.redraw))
         continue;              // Check / update every minute
      min = now / 60;
      season = *revk_season (now);
      if (*seasoncode)
         season = *seasoncode;
      if (*lights && !b.lightoverride)
      {
         int hhmm = t.tm_hour * 100 + t.tm_min;
         showlights (lighton == lightoff || (lighton < lightoff && lighton <= hhmm && lightoff > hhmm)
                     || (lightoff < lighton && (lighton <= hhmm || lightoff > hhmm)) ? lights : "");
      }
      if ((poslat || poslon || *postown) && *weatherapi)
      {                         // Weather
         char *url;
         if (*postown)
            asprintf (&url, "http://api.weatherapi.com/v1/current.json?key=%s&q=%s", weatherapi, postown);
         else
            asprintf (&url, "http://api.weatherapi.com/v1/current.json?key=%s&q=%f,%f", weatherapi,
                      (float) poslat / 10000000, (float) poslon / 1000000);
         ESP_LOGE (TAG, "%s", url);
         file_t *w = download (url, NULL);
         free (url);
         if (w && w->data && w->json)
         {
            if (w->cache > now + 3600)
               w->cache = now + 3600;
            jo_t j = jo_parse_mem (w->data, w->size);
            if (j)
            {
               if (weather)
                  jo_free (&weather);
               weather = j;
            }
         }
      }
      b.redraw = 0;
      // Image
      epd_lock ();
      if (refresh && now / refresh != fresh)
      {                         // Periodic refresh, e.g.once a day
         fresh = now / refresh;
         gfx_refresh ();
      }
      if (gfxnight && t.tm_hour >= 2 && t.tm_hour < 4)
      {
         gfx_refresh ();        // Full update
         b.redraw = 1;
      }
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
         gfx_colour (widgetk[w] == REVK_SETTINGS_WIDGETK_NORMAL || widgetk[w] == REVK_SETTINGS_WIDGETK_MASK ? 'K' : 'W');
         gfx_background (widgetk[w] == REVK_SETTINGS_WIDGETK_NORMAL || widgetk[w] == REVK_SETTINGS_WIDGETK_MASKINVERT ? 'W' : 'K');
         //if (widgett[w] || *widgetc[w]) ESP_LOGE (TAG, "Widget %2d X=%03d Y=%03d A=%02X F=%c B=%c", w + 1, gfx_x (), gfx_y (), gfx_a (), gfx_f (), gfx_b ());
         // Content substitutions
         char *c = dollar (widgetc[w], now);
         switch (widgett[w])
         {
         case REVK_SETTINGS_WIDGETT_TEXT:
            if (*c)
            {
               gfx_pos_t s = widgets[w];
               if (!s)
                  s = 5;
               gfx_text (s, "%s", c);
            }
            break;
         case REVK_SETTINGS_WIDGETT_BLOCKS:
            if (*c)
            {
               gfx_pos_t s = widgets[w];
               if (!s)
                  s = 4;
               gfx_blocky (s, "%s", c);
            }
            break;
         case REVK_SETTINGS_WIDGETT_DIGITS:
            if (*c)
            {
               gfx_pos_t s = widgets[w];
               if (!s)
                  s = 4;
               gfx_7seg (s, "%s", c);
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
                     i = download (url, ".png");
                  }
                  if (!i || !i->size)
                  {
                     strcpy (s, s + 1);
                     i = download (url, ".png");
                  }

               } else
                  i = download (c, ".png");
               if (i && i->size && i->w && i->h)
               {
                  gfx_pos_t ox,
                    oy;
                  gfx_draw (i->w, i->h, 0, 0, &ox, &oy);
                  plot (i, ox, oy);

               }
            }
            break;
         case REVK_SETTINGS_WIDGETT_QR:
            gfx_qr (c, widgets[w]);
            break;
         case REVK_SETTINGS_WIDGETT_HLINE:
            {
               gfx_pos_t s = widgets[w] ? : gfx_width ();
               if (widgeth[w] == REVK_SETTINGS_WIDGETH_CENTRE)
                  x -= s / 2;
               else if (widgeth[w] == REVK_SETTINGS_WIDGETH_RIGHT)
                  x -= s;
               gfx_line (x, y, x + s, y, 255);
            }
            break;
         case REVK_SETTINGS_WIDGETT_VLINE:
            {
               gfx_pos_t s = widgets[w] ? : gfx_height ();
               if (widgetv[w] == REVK_SETTINGS_WIDGETV_MIDDLE)
                  y -= s / 2;
               else if (widgetv[w] == REVK_SETTINGS_WIDGETV_BOTTOM)
                  y -= s;
               gfx_line (x, y, x, y + s, 255);
            }
            break;
         case REVK_SETTINGS_WIDGETT_BINS:
            extern void widget_bins (int8_t, const char *);
            widget_bins (widgets[w], c);
            break;
         }
         if (c != widgetc[w])
            free (c);
      }
      epd_unlock ();
      snmp_tx ();
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
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_VLINE)
      add (NULL, "widgeth");
   add (NULL, "widgetx");
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_HLINE)
      add (NULL, "widgetv");
   add (NULL, "widgety");
   const char *p = NULL;
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_TEXT || widgett[page - 1] == REVK_SETTINGS_WIDGETT_BLOCKS)
      p = "Font size<br>(-ve for descenders)";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_DIGITS || widgett[page - 1] == REVK_SETTINGS_WIDGETT_BINS)
      p = "Font size";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_HLINE)
      p = "Line width";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_VLINE)
      p = "Line height";
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_IMAGE)
      add (p, "widgets");
   p = NULL;
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE)
      p = "PNG Image URL";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_BINS)
      p = "Bins data JSON URL";
   else if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_QR)
      p = "QR code content";
   if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_VLINE && widgett[page - 1] != REVK_SETTINGS_WIDGETT_HLINE)
      add (p, "widgetc");

   // Extra fields
   if (!strncmp (widgetc[page - 1], "$WEATHER.", 9))
   {
      revk_web_setting (req, NULL, "weatherapi");
      revk_web_setting (req, NULL, "postown");
   }
   if (!strcmp (widgetc[page - 1], "$SUNRISE") || !strcmp (widgetc[page - 1], "$SUNSET")
       || !strncmp (widgetc[page - 1], "$WEATHER.", 9))
   {
      revk_web_setting (req, NULL, "poslat");
      revk_web_setting (req, NULL, "poslon");
   }
   if (!strcmp (widgetc[page - 1], "$WIFI") || !strcmp (widgetc[page - 1], "$SSID"))
      revk_web_setting (req, NULL, "qrssid");
   if (!strcmp (widgetc[page - 1], "$WIFI") || !strcmp (widgetc[page - 1], "$PASS"))
      revk_web_setting (req, NULL, "qrpass");
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE && strchr (widgetc[page - 1], '*'))
      revk_web_setting (req, NULL, "seasoncode");
   if (!strcmp (widgetc[page - 1], "$COUNTDOWN"))
      revk_web_setting (req, NULL, "refdate");
   if (!strncmp (widgetc[page - 1], "$SNMP", 5))
      revk_web_setting (req, NULL, "snmphost");

   // Notes
   if (widgett[page - 1] == REVK_SETTINGS_WIDGETT_IMAGE)
      revk_web_setting_info (req, "URL should be http://, and can include * for season character");
   else if (widgett[page - 1] != REVK_SETTINGS_WIDGETT_BINS)
      revk_web_setting_info (req,
                             "Content can also be <tt>$</tt> and various fields like <tt>$TIME</tt>. See manual for more details");
}
