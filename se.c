// Solar Edge modbus

#ifndef	CONFIG_IDF_TARGET       // Linux based test code
#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

void
se_task (void *x)
{
   char *host = x;
   while (1)
   {
      struct addrinfo base = {.ai_family = PF_UNSPEC,.ai_socktype = SOCK_STREAM };
      struct addrinfo *a = 0,
         *t;
      if (getaddrinfo (host, "1502", &base, &a) || !a)
      {
#ifndef	CONFIG_IDF_TARGET       // Linux based test code
         errx (1, "Cannot look up %s", host);
#else
         ESP_LOGE (TAG, "Cannot look up %s", host);
#endif
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
#ifndef	CONFIG_IDF_TARGET       // Linux based test code
         errx (1, "Cannot connect to %s", host);
#else
         ESP_LOGE (TAG, "Cannot connect to %s", host);
#endif
         sleep (10);
         continue;
      }

      void modbus_get (uint16_t reg, uint8_t regs, void *buf)
      {
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
#ifdef DUMP
         fprintf (stderr, "Tx");
         for (int i = 0; i < sizeof (req); i++)
            fprintf (stderr, " %02X", req[i]);
         fprintf (stderr, "\n");
#endif
         int l;
         if ((l = write (s, req, sizeof (req))) != sizeof (req))
         {                      // Fail
            if (l <= 0)
            {
               close (s);
               s = -1;
            }
            // TODO error handling
            warnx ("Bad tx %d", l);
         }
         if ((l = read (s, req, 6)) != 6)
         {                      // Fail
            if (l <= 0)
            {
               close (s);
               s = -1;
            }
            // TODO error handling
            warnx ("Bad rx %d", l);
         }
         if ((req[0] << 8) + req[1] != tag)
         {                      // Bad reply
            // TODO error handling
            warnx ("Bad tag %04X/%04X", (req[0] << 8) + req[1], tag);
         }
         if (req[2] || req[3])
         {                      // Bad protocol
            // TODO error handling
            warnx ("Bad protocol %02X%02X", req[2], req[3]);
         }
         uint16_t len = (req[4] << 8) + req[5];
         if (len != regs * 2 + 3)
         {                      // Unexpected len
            // TODO error handling (non fatal, read data)
            warnx ("Bad len %d", len);
         } else
         {
            if ((l = read (s, req + 6, 3)) != 3)
            {                   // Expect 3 bytes before data
               if (l <= 0)
               {
                  close (s);
                  s = -1;
               }
               warnx ("Bad rx 3 bytes %d", l);
            } else if ((l = read (s, buf, len - 3)) != len - 3)
            {                   // Bad read
               if (l <= 0)
               {
                  close (s);
                  s = -1;
               }
               warnx ("Bad rx buf %d/%d", l, len - 3);
            } else
            {
               if (req[7] != 3)
               {                // bad function
                  // TODO error handling
                  warnx ("Bad function %02X", req[7]);
               }
#ifdef DUMP
               fprintf (stderr, "Rx");
               for (int i = 0; i < 11; i++)
                  fprintf (stderr, " %02X", req[i]);
               for (int i = 0; i < len - 3; i++)
                  fprintf (stderr, " %02X", ((uint8_t *) buf)[i]);
               fprintf (stderr, "\n");
#endif
            }
         }
      }

      void modbus_string (uint16_t reg, uint8_t len, char *buf)
      {                         // Buf must have len+1 bytes, +1 if len is odd
         *buf = 0;
         modbus_get (reg, (len + 1) / 2, buf);
         buf[len] = 0;
         warnx ("%04X: %s", reg, buf);
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

      char manufacturer[17];
      modbus_string (0x9c44, sizeof (manufacturer) - 1, manufacturer);
      char model[17];
      modbus_string (0x9c54, sizeof (model) - 1, model);
      char version[9];
      modbus_string (0x9c6c, sizeof (version) - 1, version);
      char serial[17];
      modbus_string (0x9c74, sizeof (serial) - 1, serial);

      while (s >= 0)
      {

         warnx ("Voltage %u %d", modbus_16 (0x9c8c), modbus_16d (0x9c92));
         warnx ("Power AC %u %d", modbus_16 (0x9c93), modbus_16d (0x9c94));
         warnx ("Power DC %u %d", modbus_16 (0x9ca4), modbus_16d (0x9ca5));
         warnx ("Energy %u %d", modbus_32 (0x9c9d), modbus_16d (0x9c9f));
         warnx ("Freq %u %d", modbus_16 (0x9c95), modbus_16d (0x9c96));

         sleep (1);
      }
      close (s);
      break;
   }
}

#ifndef	CONFIG_IDF_TARGET       // Linux based test code
int debug = 0;

int
main (int argc, const char *argv[])
{
   const char *host = NULL;
   poptContext optCon;          // context for parsing command-line options
   {                            // POPT
      const struct poptOption optionsTable[] = {
         {"host", 'h', POPT_ARG_STRING, &host, 0, "Hostname", "name/IP"},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
         POPT_AUTOHELP {}
      };

      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp (optCon, "");

      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

      if (poptPeekArg (optCon) && !host)
         host = poptGetArg (optCon);

      if (poptPeekArg (optCon) || !host)
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
   }

   se_task ((void *) host);

   poptFreeContext (optCon);
   return 0;
}
#endif
