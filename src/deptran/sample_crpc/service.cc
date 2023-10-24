
#include "service.h"
#include "server.h"
#include <thread>
#include <gperftools/profiler.h>
#include <sched.h>
#include <pthread.h>

namespace janus {
thread_local bool hasPrinted2 = false;
int i = 1;      // #follower start
int g = 1;      // #followers inside same core
int c = 1;      // #core per follower
int s = 1;      // #core start
bool x = true;  // #core exclusion
bool first = false;

SampleCrpcServiceImpl::SampleCrpcServiceImpl(TxLogServer *sched)
    : sched_((SampleCrpcServer*)sched) {
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
	srand(curr_time.tv_nsec);
}

void SampleCrpcServiceImpl::CrpcAdd(const uint64_t& id, 
                        const int64_t& value1,
                        const int64_t& value2,
                        const std::vector<uint16_t>& addrChain, 
                        const std::vector<ResultAdd>& state, 
                        rrr::DeferredReply* defer){

  verify(sched_ != nullptr);
  //Log_info("*** inside SampleCrpcServiceImpl::CrpcAdd; tid: %d", gettid());
  if (!hasPrinted2) {
      thread_local pid_t t = gettid();
      Log_info("tid of non-leader is %d", t);
      if (addrChain.size() > 1) {
        thread_local cpu_set_t cs;
        CPU_ZERO(&cs);
        for (int k = 0; k < c; k++)
          CPU_SET(s+k-((!x) ? ((i-1) / g) : 0 ), &cs);
        s += (i % g == 0) ? c : 0;
        i++;
        verify(sched_setaffinity(t, sizeof(cs), &cs) == 0);
      }
      hasPrinted2 = true;  // Update the static variable
  }

  // Log_info("*** inside SampleCrpcServiceImpl::CrpcAdd; cp 2 tid: %d", gettid());
  //Log_info("*** --- inside SampleCrpcServiceImpl::CrpcAdd, entering OnCRPC3");
  Coroutine::CreateRun([&] () {
      sched_->OnCRPC3(id,
                    value1,
                    value2,
                    addrChain,
                    state);
      // Log_info("*** inside SampleCrpcServiceImpl::CrpcAdd; cp 3 tid: %d", gettid());
      defer->reply();
      // Log_info("*** inside SampleCrpcServiceImpl::CrpcAdd; cp 4 tid: %d", gettid());
  });

  // Log_info("*** returning from SampleCrpcServiceImpl::CrpcAdd; tid: %d", gettid());
}

void SampleCrpcServiceImpl::BroadcastAdd(const int64_t& value1,
                                        const int64_t& value2,
                                        int64_t *result,
                                        rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  if (!hasPrinted2) {
      thread_local pid_t t = gettid();
      Log_info("tid of non-leader is %d", t);
      thread_local cpu_set_t cs;
      CPU_ZERO(&cs);
      for (int k = 0; k < c; k++)
        CPU_SET(s+k-((!x) ? ((i-1) / g) : 0 ), &cs);
      s += (i % g == 0) ? c : 0;
      i++;
      verify(sched_setaffinity(t, sizeof(cs), &cs) == 0);
      hasPrinted2 = true;  // Update the static variable
  }

  Coroutine::CreateRun([&] () {
    sched_->OnAdd(value1,
                            value2,
                            result,
                            std::bind(&rrr::DeferredReply::reply, defer));  // #profile - 3.42%

  });

  // Log_info("==== returning from SampleCrpcServiceImpl::Add");
	
}

} // namespace janus;
