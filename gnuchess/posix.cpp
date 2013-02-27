/* posix.cpp

   GNU Chess engine

   Copyright (C) 2001-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// posix.cpp

// includes

#include <cerrno>
#include <cstdio> // REMOVE ME?
#include <cstdlib>
#include <cstring>
#include <ctime>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#  include <winsock2.h>
#else // assume POSIX
#  include <sys/resource.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#include "posix.h"
#include "util.h"

namespace engine {

// Streams used to communicate with the interface

extern FILE *interface_input;
extern FILE *interface_output;

// prototypes

#if !defined(_WIN32) && !defined(_WIN64)
static double duration (const struct timeval * tv);
#endif

// functions

// input_available()

bool input_available() {
   int val;
   fd_set set[1];
   struct timeval time_val[1];

   FD_ZERO(set);
   FD_SET(_fileno(interface_input),set);

   time_val->tv_sec = 0;
   time_val->tv_usec = 0;

   val = select(_fileno(interface_input)+1,set,NULL,NULL,time_val);
   if (val == -1 && errno != EINTR) {
      my_fatal("input_available(): select(): %s\n",strerror(errno));
   }

   return val > 0;
}

// now_real()

double now_real() {

#if defined(_WIN32) || defined(_WIN64)

   return double(GetTickCount()) / 1000.0;

#else // assume POSIX

   struct timeval tv[1];
   struct timezone tz[1];

   tz->tz_minuteswest = 0;
   tz->tz_dsttime = 0; // DST_NONE not declared in GNU/Linux

   if (gettimeofday(tv,tz) == -1) { // tz needed at all?
      my_fatal("now_real(): gettimeofday(): %s\n",strerror(errno));
   }

   return duration(tv);

#endif
}

// now_cpu()

double now_cpu() {

#if defined(_WIN32) || defined(_WIN64)

   return double(clock()) / double(CLOCKS_PER_SEC); // OK if CLOCKS_PER_SEC is small enough

#else // assume POSIX

   struct rusage ru[1];

   if (getrusage(RUSAGE_SELF,ru) == -1) {
      my_fatal("now_cpu(): getrusage(): %s\n",strerror(errno));
   }

   return duration(&ru->ru_utime);

#endif
}

// duration()

#if !defined(_WIN32) && !defined(_WIN64)

static double duration(const struct timeval * tv) {

   ASSERT(tv!=NULL);

   return double(tv->tv_sec) + double(tv->tv_usec) * 1E-6;
}

#endif

}  // namespace engine

// end of posix.cpp

