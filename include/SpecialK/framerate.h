/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#ifndef __SK__FRAMERATE_H__
#define __SK__FRAMERATE_H__

struct IUnknown;
#include <Unknwnbase.h>
#include <Windows.h>

#include <cstdint>
#include <cmath>
#include <forward_list>

template <class T, class U>
constexpr T narrow_cast(U&& u)
{
  return static_cast<T>(std::forward<U>(u));
}

static constexpr auto
  long_double_cast =
  [](auto val) ->
    long double
    {
      return
        static_cast <long double> (val);
    };


namespace SK
{
  namespace Framerate
  {
    void Init     (void);
    void Shutdown (void);

    void Tick     (long double& dt, LARGE_INTEGER& now);

    class Limiter {
    public:
      Limiter (long double target = 60.0l);

      ~Limiter (void) = default;

      void            init            (long double target);
      void            wait            (void);
      bool        try_wait            (void); // No actual wait, just return
                                              //  whether a wait would have occurred.

      void        set_limit           (long double target);
      long double get_limit           (void) noexcept { return fps; };

      long double effective_frametime (void);

      int32_t     suspend             (void) noexcept { return ++limit_behavior; }
      int32_t     resume              (void) noexcept { return --limit_behavior; }

      void        reset (bool full = false) noexcept {
        if (full) full_restart = true;
        else           restart = true;
      }

    private:
      bool          restart      = false;
      bool          full_restart = false;
      bool          background   = false;

      long double   ms           = 0.0L,
                    fps          = 0.0L,
                    effective_ms = 0.0L;

      ULONGLONG     ticks_per_frame = 0ULL;

      volatile
        LONG64      time   = { },
                    start  = { },
                    next   = { },
                    last   = { },
                    freq   = { };

      volatile
          LONG      frames = 0;

#define LIMIT_APPLY     0
#define LIMIT_UNDEFLOW  (limit_behavvior < 0)
#define LIMIT_SUSPENDED (limit_behavivor > 0)

      // 0 = Limiter runs, < 0 = Reference Counting Bug (dumbass)
      //                   > 0 = Temporarily Ignore Limits
       int32_t      limit_behavior =
                    LIMIT_APPLY;
    };

    using EventCounter = class EventCounter_V1;

    class EventCounter_V1
    {
    public:
      class SleepStats
      {
      public:
        volatile ULONG attempts   = 0UL,
                       rejections = 0UL;

        struct
        {
          volatile LONG deprived = 0ULL,
                        allowed  = 0ULL;
        } time;


        void sleep (DWORD dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedAdd       (&time.allowed,  narrow_cast <ULONG> (dwMilliseconds)); }
        void wake  (DWORD dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedIncrement (&rejections);
                                            InterlockedAdd       (&time.deprived, narrow_cast <ULONG> (dwMilliseconds)); }
      };

      SleepStats& getMessagePumpStats  (void) noexcept { return message_pump;  }
      SleepStats& getRenderThreadStats (void) noexcept { return render_thread; }
      SleepStats& getMicroStats        (void) noexcept { return micro_sleep;   }
      SleepStats& getMacroStats        (void) noexcept { return macro_sleep;   }

    protected:
      SleepStats message_pump, render_thread,
                 micro_sleep,  macro_sleep;
    } extern *events;


    static inline EventCounter* GetEvents  (void) noexcept { return events; }
                  Limiter*      GetLimiter (void);

    class Stats {
    public:
      static LARGE_INTEGER freq;

      Stats (void) noexcept {
        QueryPerformanceFrequency (&freq);
      }

    #define MAX_SAMPLES 120
      struct sample_t {
        long double   val  = 0.0;
        LARGE_INTEGER when = { 0ULL };
      } data [MAX_SAMPLES];
      int    samples       = 0;

      void addSample (long double sample, LARGE_INTEGER time) noexcept
      {
        data [samples % MAX_SAMPLES].val  = sample;
        data [samples % MAX_SAMPLES].when = time;

        samples++;
      }

      long double calcMean (long double seconds = 1.0L);

      long double calcMean (LARGE_INTEGER start) noexcept
      {
        long double mean = 0.0L;

        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            ++samples_used;
            mean += datum.val;
          }
        }

        return mean / static_cast <long double> (samples_used);
      }

      long double calcSqStdDev (long double mean, long double seconds = 1.0L);

      long double calcSqStdDev (long double mean, LARGE_INTEGER start) noexcept
      {
        long double sd = 0.0;

        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            sd += (datum.val - mean) *
                  (datum.val - mean);
            samples_used++;
          }
        }

        return sd / static_cast <long double> (samples_used);
      }

      long double calcMin (long double seconds = 1.0L);

      long double calcMin (LARGE_INTEGER start) noexcept
      {
        long double min = INFINITY;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val < min)
              min = datum.val;
          }
        }

        return min;
      }

      long double calcMax (long double seconds = 1.0L);

      long double calcMax (LARGE_INTEGER start) noexcept
      {
        long double max = -INFINITY;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > max)
              max = datum.val;
          }
        }

        return max;
      }

      int calcHitches (long double tolerance, long double mean, long double seconds = 1.0);

      int calcHitches (long double tolerance, long double mean, LARGE_INTEGER start) noexcept
      {
        int hitches = 0;

    #if 0
        for (int i = 1; i < MAX_SAMPLES; i++) {
          if (data [i    ].when.QuadPart >= start.QuadPart &&
              data [i - 1].when.QuadPart >= start.QuadPart) {
            if ((data [i].val + data [i - 1].val) / 2.0 > (tolerance * data [i - 1].val) ||
                (data [i].val + data [i - 1].val) / 2.0 > (tolerance * data [i].val))
              hitches++;
          }
        }

        // Handle wrap-around on the final sample
        if (data [0              ].when.QuadPart >= start.QuadPart &&
            data [MAX_SAMPLES - 1].when.QuadPart >= start.QuadPart &&
            data [0].when.QuadPart > data [MAX_SAMPLES -1].when.QuadPart) {
          if ((data [MAX_SAMPLES - 1].val - data [0].val) > (tolerance * data [MAX_SAMPLES - 1].val))
            hitches++;
        }
    #else
        bool last_late = false;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > tolerance * mean)
            {
              if (! last_late)
                hitches++;
              last_late = true;
            }

            else
            {
              last_late = false;
            }
          }
        }
    #endif

        return hitches;
      }

      int calcNumSamples (long double seconds = 1.0);

      int calcNumSamples (LARGE_INTEGER start) noexcept
      {
        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            samples_used++;
          }
        }

        return samples_used;
      }
    };
  };
};

using QueryPerformanceCounter_pfn = BOOL (WINAPI *)(_Out_ LARGE_INTEGER *lpPerformanceCount);

BOOL
WINAPI
SK_QueryPerformanceCounter (_Out_ LARGE_INTEGER *lpPerformanceCount);

using  Sleep_pfn = void (WINAPI *)(DWORD dwMilliseconds);
extern Sleep_pfn
       Sleep_Original;

using  SleepEx_pfn = DWORD (WINAPI *)(DWORD dwMilliseconds,
                                      BOOL  bAlertable);
extern SleepEx_pfn
       SleepEx_Original;

extern LARGE_INTEGER SK_GetPerfFreq (void);
extern LARGE_INTEGER SK_QueryPerf   (void);

static auto SK_CurrentPerf =
 []{
     LARGE_INTEGER                time;
     SK_QueryPerformanceCounter (&time);
     return                       time;
   };

static auto SK_DeltaPerf =
 [](auto delta, auto freq)->
  LARGE_INTEGER
   {
     LARGE_INTEGER time = SK_CurrentPerf ();

     time.QuadPart -= gsl::narrow_cast <LONGLONG> (delta * freq);

     return time;
   };

static auto SK_DeltaPerfMS =
 [](auto delta, auto freq)->
  double
   {
     return
       1000.0 * (double)(SK_DeltaPerf (delta, freq).QuadPart) /
                (double)SK_GetPerfFreq           ().QuadPart;
   };

extern int __SK_FramerateLimitApplicationSite;



using NtQueryTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  OUT PULONG              MinimumResolution,
  OUT PULONG              MaximumResolution,
  OUT PULONG              CurrentResolution
);

using NtSetTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  IN  ULONG               DesiredResolution,
  IN  BOOLEAN             SetResolution,
  OUT PULONG              CurrentResolution
);

typedef NTSTATUS (WINAPI *NtDelayExecution_pfn)(
  IN  BOOLEAN        Alertable,
  IN  PLARGE_INTEGER DelayInterval
);


struct IDXGIOutput;

using WaitForVBlank_pfn = HRESULT (STDMETHODCALLTYPE *)(
  IDXGIOutput *This
);

typedef
NTSTATUS (NTAPI *NtWaitForSingleObject_pfn)(
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout    // Microseconds
);


typedef enum _OBJECT_WAIT_TYPE {
  WaitAllObject,
  WaitAnyObject
} OBJECT_WAIT_TYPE,
*POBJECT_WAIT_TYPE;

typedef
NTSTATUS (NTAPI *NtWaitForMultipleObjects_pfn)(
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL );


extern void SK_Scheduler_Init     (void);
extern void SK_Scheduler_Shutdown (void);


#endif /* __SK__FRAMERATE_H__ */