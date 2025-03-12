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
      if (getaddrinfo (host, "502", &base, &a) || !a)
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


      close (s);
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
