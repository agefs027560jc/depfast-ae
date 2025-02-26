
#include "../__dep__.h"
#include "../constants.h"
#include "coordinator.h"
#include "commo.h"
// #include <chrono>
#include "server.h"

namespace janus {

CoordinatorFpgaRaft::CoordinatorFpgaRaft(uint32_t coo_id,
                                             int32_t benchmark,
                                             ClientControlServiceImpl* ccsi,
                                             uint32_t thread_id)
    : Coordinator(coo_id, benchmark, ccsi, thread_id) {
}

bool CoordinatorFpgaRaft::IsLeader() {
   return this->sch_->IsLeader() ;
}

bool CoordinatorFpgaRaft::IsFPGALeader() {
   return this->sch_->IsFPGALeader() ;
}

void CoordinatorFpgaRaft::Forward(shared_ptr<Marshallable>& cmd,
                                   const function<void()>& func,
                                   const function<void()>& exe_callback) {
    //for(int i = 0; i < 100; i++) Log_info("inside forward");
		verify(0) ; // TODO delete it
    auto e = commo()->SendForward(par_id_, loc_id_, cmd);
    e->Wait();
    uint64_t cmt_idx = e->CommitIdx() ;
    cmt_idx_ = cmt_idx ;
    Coroutine::CreateRun([&] () {
      this->sch_->SpCommit(cmt_idx) ;
    }) ;
}


void CoordinatorFpgaRaft::Submit(shared_ptr<Marshallable>& cmd,
                                   const function<void()>& func,
                                   const function<void()>& exe_callback) {
  // Log_info("*** inside void CoordinatorFpgaRaft::Submit; tid: %d", gettid());
  if (!IsLeader()) {
    //Log_fatal("i am not the leader; site %d; locale %d",
    //          frame_->site_info_->id, loc_id_);
    Forward(cmd, func, exe_callback) ;
    return ;
  }
  
	std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(!in_submission_);
  verify(cmd_ == nullptr);
//  verify(cmd.self_cmd_ != nullptr);
  in_submission_ = true;
  cmd_ = cmd;
  verify(cmd_->kind_ != MarshallDeputy::UNKNOWN);
  commit_callback_ = func;
  GotoNextPhase();
  // Log_info("*** returning from void CoordinatorFpgaRaft::Submit");
}

void CoordinatorFpgaRaft::AppendEntries() {
    // Log_info("inside coordinator appendEntries; tid: %d", gettid());
    // std::lock_guard<std::recursive_mutex> lock(mtx_);
    verify(!in_append_entries);
    // verify(this->sch_->IsLeader()); TODO del it yidawu
    in_append_entries = true;
    Log_debug("fpga-raft coordinator broadcasts append entries, "
                  "par_id_: %lx, slot_id: %llx, lastLogIndex: %d",
              par_id_, slot_id_, this->sch_->lastLogIndex);
    /* Should we use slot_id instead of lastLogIndex and balot instead of term? */
    uint64_t prevLogIndex = this->sch_->lastLogIndex;

    /*this->sch_->lastLogIndex += 1;
    auto instance = this->sch_->GetFpgaRaftInstance(this->sch_->lastLogIndex);

    instance->log_ = cmd_;
    instance->term = this->sch_->currentTerm;*/

    /* TODO: get prevLogTerm based on the logs */
    uint64_t prevLogTerm = this->sch_->currentTerm;
		this->sch_->SetLocalAppend(cmd_, &prevLogTerm, &prevLogIndex, slot_id_, curr_ballot_) ;
		
    shared_ptr<FpgaRaftAppendQuorumEvent> sp_quorum = nullptr;
    static uint64_t count = 0;
    count++;

    
    // Log_info("*** inside void CoordinatorFpgaRaft::AppendEntries; count: %ld; tid is: %d", count, gettid());
    // if(Config::GetConfig()->get_cRPC_version() == 0 && true){
    if (Config::GetConfig()->get_cRPC_version() == 0){
      sp_quorum = commo()->BroadcastAppendEntries(par_id_,
                                                     this->sch_->site_id_,
                                                     slot_id_,
                                                     dep_id_,
                                                     curr_ballot_,
                                                     this->sch_->IsLeader(),
                                                     this->sch_->currentTerm,
                                                     prevLogIndex,
                                                     prevLogTerm,
                                                     /* ents, */
                                                     this->sch_->commitIndex,
                                                     cmd_);
    }
    else if (Config::GetConfig()->get_cRPC_version() == 1) {
      sp_quorum = commo()->crpc_ring_BroadcastAppendEntries(par_id_,
                                                     this->sch_->site_id_,
                                                     slot_id_,
                                                     dep_id_,
                                                     curr_ballot_,
                                                     this->sch_->IsLeader(),
                                                     this->sch_->currentTerm,
                                                     prevLogIndex,
                                                     prevLogTerm,
                                                     /* ents, */
                                                     this->sch_->commitIndex,
                                                     cmd_);
    }
    // else{
    //   sp_quorum = commo()->crpc_BroadcastAppendEntries(par_id_,
    //                                                  this->sch_->site_id_,
    //                                                  slot_id_,
    //                                                  dep_id_,
    //                                                  curr_ballot_,
    //                                                  this->sch_->IsLeader(),
    //                                                  this->sch_->currentTerm,
    //                                                  prevLogIndex,
    //                                                  prevLogTerm,
    //                                                  /* ents, */
    //                                                  this->sch_->commitIndex,
    //                                                  cmd_);
    // }
    

		// struct timespec start_, end_;
		// clock_gettime(CLOCK_MONOTONIC, &start_);
    // Log_info("=== waiting for quorum");
    sp_quorum->Wait();
    // Log_info("*** quorum reached");
		// struct timespec end_;
		// clock_gettime(CLOCK_MONOTONIC, &end_);

		// quorum_events_.push_back(sp_quorum);
		// Log_info("*** time of sp_quorum->Wait(): %ld", (end_.tv_sec-start_.tv_sec)* 1000000L + (end_.tv_nsec-start_.tv_nsec)/1000L);
		slow_ = sp_quorum->IsSlow();  // #profile - 2.13%
		
		long leader_time;
		std::vector<long> follower_times {};

		int total_ob = 0;
		int avg_ob = 0;
		//Log_info("begin_index: %d", commo()->begin_index);
    // Log_info("going to call commo()");
		if (commo()->begin_index >= 1000) {
			if (commo()->ob_index < 100) {
				commo()->outbounds[commo()->ob_index] = commo()->outbound;
				commo()->ob_index++;
			} else {
				for (int i = 0; i < 99; i++) {
					commo()->outbounds[i] = commo()->outbounds[i+1];
					total_ob += commo()->outbounds[i];
				}
				commo()->outbounds[99] = commo()->outbound;
				total_ob += commo()->outbounds[99];
			}
			commo()->begin_index = 0;
		} else {
			commo()->begin_index++;
		}
    // Log_info("after call commo()");
		avg_ob = total_ob/100;

		for (auto it = commo()->rpc_clients_.begin(); it != commo()->rpc_clients_.end(); it++) {
			if (avg_ob > 0 && it->second->time_ > 0) Log_info("time for %d is: %d", it->first, it->second->time_/avg_ob);
			if (it->first != loc_id_) {
				follower_times.push_back(it->second->time_);
			}
		}
		if (avg_ob > 0 && !slow_) {
			Log_debug("number of rpcs: %d", avg_ob);
			Log_debug("%d and %d", follower_times[0]/avg_ob, follower_times[1]/avg_ob);
			slow_ = follower_times[0]/avg_ob > 80000 && follower_times[1]/avg_ob > 80000;
		}

		// Log_info("slow?: %d", slow_);
    if (sp_quorum->Yes()) {
        minIndex = sp_quorum->minIndex;
				//Log_info("%d vs %d", minIndex, this->sch_->commitIndex);
        // verify(minIndex >= this->sch_->commitIndex) ; // TODO: ks-RM: uncomment and check
        committed_ = true;
        Log_debug("fpga-raft append commited loc:%d minindex:%d", loc_id_, minIndex ) ;
    }
    else if (sp_quorum->No()) {
        verify(0);
        // TODO should become a follower if the term is smaller
        //if(!IsLeader())
        {
            Forward(cmd_,commit_callback_) ;
            return ;
        }
    }
    else {
        verify(0);
    }
    // Log_info("*** returning from void CoordinatorFpgaRaft::AppendEntries");
}

void CoordinatorFpgaRaft::Commit() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  commit_callback_();
  Log_debug("fpga-raft broadcast commit for partition: %d, slot %d",
            (int) par_id_, (int) slot_id_);
  commo()->BroadcastDecide(par_id_, slot_id_, dep_id_, curr_ballot_, cmd_);
  verify(phase_ == Phase::COMMIT);
  GotoNextPhase();
}

void CoordinatorFpgaRaft::LeaderLearn() {
    // Log_info("inside CoordinatorFpgaRaft::LeaderLearn(); cp1");
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    // commit_callback_();
    uint64_t prevCommitIndex = this->sch_->commitIndex;
    // verify(minIndex >= prevCommitIndex);  // TODO: ks-RM: uncomment and check
    this->sch_->commitIndex = std::max(this->sch_->commitIndex, minIndex);
    Log_debug("fpga-raft commit for partition: %d, slot %d, commit %d minIndex %d in loc:%d", 
      (int) par_id_, (int) slot_id_, sch_->commitIndex, minIndex, loc_id_);

    /* if (prevCommitIndex < this->sch_->commitIndex) { */
    /*     auto instance = this->sch_->GetFpgaRaftInstance(this->sch_->commitIndex); */
    /*     this->sch_->app_next_(*instance->log_); */
    /* } */
    auto empty_cmd = std::make_shared<TpcEmptyCommand>();
    auto sp_m = dynamic_pointer_cast<Marshallable>(empty_cmd);
    // Log_info("Inside leader learn");
    if (Config::GetConfig()->get_cRPC_version() == 0){
      commo()->BroadcastDecide(par_id_, slot_id_, dep_id_, curr_ballot_, sp_m);
    }
    else if (Config::GetConfig()->get_cRPC_version() == 1){
      commo()->CrpcDecide(par_id_, this->sch_->site_id_, slot_id_, dep_id_, curr_ballot_, sp_m);
    }
    verify(phase_ == Phase::COMMIT);
    GotoNextPhase();
}

void CoordinatorFpgaRaft::GotoNextPhase() {
  // Log_info("*** inside CoordinatorFpgaRaft::GotoNextPhase");
  int n_phase = 4;
  int current_phase = phase_ % n_phase;
  phase_++;
  switch (current_phase) {
    case Phase::INIT_END:
      if (IsLeader()) {
        // Log_info("*** inside GotoNextPhase->INIT_END->IsLeader()");
        phase_++; // skip prepare phase for "leader"
        verify(phase_ % n_phase == Phase::ACCEPT);
        AppendEntries();
        phase_++;
        verify(phase_ % n_phase == Phase::COMMIT);
      } else {
        // TODO
        // // Log_info("*** inside GotoNextPhase->INIT_END->not IsLeader()");
        verify(0);
        Forward(cmd_,commit_callback_) ;
        phase_ = Phase::COMMIT;
      }
    case Phase::ACCEPT:
      // // Log_info("*** inside GotoNextPhase->ACCEPT");
      verify(phase_ % n_phase == Phase::COMMIT);
      if (committed_) {
        LeaderLearn();
      } else {
        // verify(0);
        // Forward(cmd_,commit_callback_) ;
        phase_ = Phase::COMMIT;
      }
      break;
    case Phase::PREPARE:
      // Log_info("*** inside GotoNextPhase->PREPARE");
      verify(phase_ % n_phase == Phase::ACCEPT);
      AppendEntries();
      break;
    case Phase::COMMIT:
      // Log_info("*** inside GotoNextPhase->COMMIT");
      // do nothing.
      break;
    default:
      verify(0);
  }
}

} // namespace janus
