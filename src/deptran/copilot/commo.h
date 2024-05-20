#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"
#include "server.h"

namespace janus {

class CopilotFastAcceptQuorumEvent : public QuorumEvent {
  // TODO: use OrEvent to express fastpath vs. slowpath?
  vector<uint64_t> ret_deps_;
  int32_t n_fastac_ok_{0};
  int32_t n_fastac_reply_{0};
 public:
  // using QuorumEvent::QuorumEvent;
  CopilotFastAcceptQuorumEvent(int n_total, int quorum)
      : QuorumEvent(n_total, quorum) {
    ret_deps_.reserve(n_total);
  }

  void FeedResponse(bool y, bool ok);
  void FeedRetDep(uint64_t dep);
  uint64_t GetFinalDep();

  bool FastYes();
  bool FastNo();
};

class CopilotAcceptQuorumEvent : public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;

  void FeedResponse(bool y) {
    if (y)
      VoteYes();
    else
      VoteNo();
  }
};

class CopilotPrepareQuorumEvent : public QuorumEvent {
  vector<vector<CopilotData> > ret_cmds_by_status_;

 public:
  // using QuorumEvent::QuorumEvent;
  bool committed_seen_ = false;
  CopilotPrepareQuorumEvent(int n_total, int quorum)
      : QuorumEvent(n_total, quorum), ret_cmds_by_status_(n_status) {}

  void FeedResponse(bool y) {
    if (y)
      VoteYes();
    else
      VoteNo();
  }

  void FeedRetCmd(ballot_t ballot,
                  uint64_t dep,
                  uint8_t is_pilot, slotid_t slot,
                  shared_ptr<Marshallable> cmd,
                  enum Status status);
  size_t GetCount(enum Status status);
  vector<CopilotData>& GetCmds(enum Status status);
  bool IsReady() override;
  void Show();
};

/**
 * A "Quorum Event" which has no quorum
 * Used for those who don't need quorum reply
 */
class CopilotFakeQuorumEvent : public QuorumEvent {
 public:
  CopilotFakeQuorumEvent(int n_total)
    : QuorumEvent(n_total, 0) {}

  void FeedResponse() { VoteYes(); }
  bool IsReady() override { return true; }
};

class CopilotCommo : public Communicator {
friend class CopilotProxy;
 public:
  static int fastQuorumSize(int total);
  static int quorumSize(int total);
  static int maxFailure(int total);

 public:
  std::unordered_map<uint64_t, shared_ptr<CopilotFastAcceptQuorumEvent>> cRPCEvents {};
  CopilotCommo() = delete;
  CopilotCommo(PollMgr *);

  shared_ptr<CopilotPrepareQuorumEvent>
  BroadcastPrepare(parid_t par_id,
                   uint8_t is_pilot,
                   slotid_t slot_id,
                   ballot_t ballot);
  
  shared_ptr<CopilotFastAcceptQuorumEvent>
  BroadcastFastAccept(parid_t par_id,
                      uint8_t is_pilot,
                      slotid_t slot_id,
                      ballot_t ballot,
                      uint64_t dep,
                      shared_ptr<Marshallable> cmd);
  
  shared_ptr<CopilotFastAcceptQuorumEvent>
  CrpcFastAccept(parid_t par_id,
                 siteid_t leader_site_id,
                 uint8_t is_pilot,
                 slotid_t slot_id,
                 ballot_t ballot,
                 uint64_t dep,
                 shared_ptr<Marshallable> cmd);

  void CrpcProxyFastAccept(const uint64_t& id,
                           const uint8_t& is_pilot,
                           const uint64_t& slot,
                           const ballot_t& ballot,
                           const uint64_t& dep,
                           const MarshallDeputy& cmd,
                           const struct DepId& dep_id,
                           const std::vector<uint16_t>& addrChain,
                           const vector<CopilotMessage>& state);

  shared_ptr<CopilotAcceptQuorumEvent>
  BroadcastAccept(parid_t par_id,
                  uint8_t is_pilot,
                  slotid_t slot_id,
                  ballot_t ballot,
                  uint64_t dep,
                  shared_ptr<Marshallable> cmd);
  
  shared_ptr<CopilotFakeQuorumEvent>
  BroadcastCommit(parid_t par_id,
                       uint8_t is_pilot,
                       slotid_t slot_id,
                       uint64_t dep,
                       shared_ptr<Marshallable> cmd);
  
  shared_ptr<CopilotFakeQuorumEvent>
  CrpcCommit(const parid_t par_id,
             const siteid_t leader_site_id,
             const uint8_t is_pilot,
             const slotid_t slot_id,
             const uint64_t dep,
             const shared_ptr<Marshallable> cmd);

  void CrpcProxyCommit(const uint8_t is_pilot,
                  const slotid_t slot_id,
                  const uint64_t dep,
                  const MarshallDeputy& cmd,
                  const std::vector<uint16_t>& addrChain,
                  const vector<CopilotMessage> state);
};

}  // namespace janus