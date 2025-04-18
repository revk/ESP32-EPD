
typedef struct file_s
{
   struct file_s *next;         // Next file in chain
   char *url;                   // URL as passed to download
   uint32_t cache;              // Cache until this uptiome
   time_t changed;              // Last changed
   uint32_t size;               // File size
   uint32_t w;                  // PNG width
   uint32_t h;                  // PNG height
   uint8_t *data;               // File data
   uint8_t card:1;              // We have tried card
   uint8_t json:1;              // Is JSON
} file_t;

extern const char *const longday[];
extern const char *const shortday[];

file_t *download (char *url,const char *suffix);
void plot (file_t * i, gfx_pos_t ox, gfx_pos_t oy,uint8_t invert);
void setlights(const char *rgb);
