#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"
#include <chrono>
#include <ctime>

namespace janus {

class TxData;

class PaxosPrepareQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
//  ballot_t max_ballot_{0};
  bool HasAcceptedValue() {
    // TODO implement this
    return false;
  }
  void FeedResponse(bool y) {
    if (y) {
      VoteYes();
    } else {
      VoteNo();
    }
  }


};

class PaxosAcceptQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
  void FeedResponse(bool y) {
    if (y) {
      VoteYes();
    } else {
      VoteNo();
    }
  }
};

class MultiPaxosCommo : public Communicator {
 public:
  std::unordered_map<uint64_t, shared_ptr<PaxosAcceptQuorumEvent>> cRPCEvents {};

  MultiPaxosCommo() = delete;
  MultiPaxosCommo(PollMgr*);

  shared_ptr<PaxosPrepareQuorumEvent>
  SendForward(parid_t par_id,
              uint64_t follower_id,
              uint64_t dep_id,
              shared_ptr<Marshallable> cmd);

  shared_ptr<PaxosPrepareQuorumEvent>
  BroadcastPrepare(parid_t par_id,
                   slotid_t slot_id,
                   ballot_t ballot);
  void BroadcastPrepare(parid_t par_id,
                        slotid_t slot_id,
                        ballot_t ballot,
                        const function<void(Future *fu)> &callback);
  shared_ptr<PaxosAcceptQuorumEvent>
  BroadcastAccept(parid_t par_id,
                  slotid_t slot_id,
                  ballot_t ballot,
                  shared_ptr<Marshallable> cmd);

  shared_ptr<PaxosAcceptQuorumEvent>
  CrpcAccept(parid_t par_id,
                  siteid_t leader_site_id,
                  slotid_t slot_id,
                  ballot_t ballot,
                  shared_ptr<Marshallable> cmd);

  void CrpcProxyAccept(const uint64_t& id,
                      const slotid_t slot_id,
		                  const uint64_t time,
                      const ballot_t ballot,
                      const MarshallDeputy& cmd,
                      const std::vector<uint16_t>& addrChain,
                      const vector<PaxosMessage> state);

  void BroadcastAccept(parid_t par_id,
                       slotid_t slot_id,
                       ballot_t ballot,
                       shared_ptr<Marshallable> cmd,
                       const function<void(Future*)> &callback);
  void BroadcastDecide(const parid_t par_id,
                       const slotid_t slot_id,
                       const ballot_t ballot,
                       const shared_ptr<Marshallable> cmd);

  void CrpcDecide(const parid_t par_id,
                  const siteid_t leader_site_id,
                  const slotid_t slot_id,
                  const ballot_t ballot,
                  const shared_ptr<Marshallable> cmd);

  void CrpcProxyDecide(const parid_t par_id,
                       const slotid_t slot_id,
                       const ballot_t ballot,
                       const MarshallDeputy& cmd,
                       const std::vector<uint16_t>& addrChain,
                       const vector<PaxosMessage> state);
};

} // namespace janus
