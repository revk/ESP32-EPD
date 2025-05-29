
typedef struct file_s
{
   struct file_s *next;         // Next file in chain
   char *url;                   // URL as passed to download
   const char *suffix;          // Default suffix
   uint32_t cachetime;		// Cache time
   time_t changed;              // Last change (uptime)
   time_t cached;               // Cache/retry (uptime)
   uint32_t size;               // File size
   uint32_t w;                  // PNG width
   uint32_t h;                  // PNG height
   uint8_t *data;               // File data
   uint8_t backoff;	// Backoff retry
   uint8_t reload:1;		// File in use after cache expired, so reload
   uint8_t card:1;              // We have tried card
   uint8_t json:1;              // Is JSON
} file_t;
extern SemaphoreHandle_t file_mutex;

extern const char *const longday[];
extern const char *const shortday[];

file_t *download (char *url,const char *suffix,char force);
void plot (file_t * i, gfx_pos_t ox, gfx_pos_t oy,uint8_t invert);
void setlights(const char *rgb);
