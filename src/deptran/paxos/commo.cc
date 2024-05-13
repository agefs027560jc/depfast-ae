
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

MultiPaxosCommo::MultiPaxosCommo(PollMgr* poll) : Communicator(poll) {
  // Log_info("Tracepath i");
//  verify(poll != nullptr);
}

shared_ptr<PaxosPrepareQuorumEvent>
MultiPaxosCommo::SendForward(parid_t par_id,
                             uint64_t follower_id,
                             uint64_t dep_id,
                             shared_ptr<Marshallable> cmd){
  // Log_info("Tracepath ii");
  auto e = Reactor::CreateSpEvent<PaxosPrepareQuorumEvent>(1, 1);
  auto src_coroid = e->GetCoroId();
  auto leader_id = LeaderProxyForPartition(par_id).first;
  auto leader_proxy = (MultiPaxosProxy*) LeaderProxyForPartition(par_id).second;

  FutureAttr fuattr;
  fuattr.callback = [e, leader_id, src_coroid, follower_id](Future* fu) {
    uint64_t coro_id = 0;
    fu->get_reply() >> coro_id;
    e->FeedResponse(1);
    Log_info("adding dependency");
    // e->add_dep(follower_id, src_coroid, leader_id, coro_id);
  };

  MarshallDeputy md(cmd);
  Future::safe_release(leader_proxy->async_Forward(md, dep_id));

  return e;
}

void MultiPaxosCommo::BroadcastPrepare(parid_t par_id,
                                       slotid_t slot_id,
                                       ballot_t ballot,
                                       const function<void(Future*)>& cb) {
  // Log_info("Tracepath iii");
  verify(0); // deprecated function
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = cb;
    Future::safe_release(proxy->async_Prepare(slot_id, ballot, fuattr));
  }
}

shared_ptr<PaxosPrepareQuorumEvent>
MultiPaxosCommo::BroadcastPrepare(parid_t par_id,
                                  slotid_t slot_id,
                                  ballot_t ballot) {
  // Log_info("Tracepath iv");
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<PaxosPrepareQuorumEvent>(n, n/2+1);
  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];

  WAN_WAIT;
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    auto follower_id = p.first;
    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, ballot, leader_id, src_coroid, follower_id](Future* fu) {
      ballot_t b = 0;
      uint64_t coro_id = 0;
      fu->get_reply() >> b >> coro_id;
      e->FeedResponse(b==ballot);
      // e->deps[leader_id][src_coroid][follower_id].erase(-1);
      // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
      // TODO add max accepted value.
    };
    Future::safe_release(proxy->async_Prepare(slot_id, ballot, fuattr));
  }
  return e;
}

shared_ptr<PaxosAcceptQuorumEvent>
MultiPaxosCommo::BroadcastAccept(parid_t par_id,
                                 slotid_t slot_id,
                                 ballot_t ballot,
                                 shared_ptr<Marshallable> cmd) {
  // Log_info("Tracepath v");
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<PaxosAcceptQuorumEvent>(n, n/2+1);
//  auto e = Reactor::CreateSpEvent<PaxosAcceptQuorumEvent>(n, n);

  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first; // might need to be changed to coordinator's id
  vector<Future*> fus;
  auto start = chrono::system_clock::now();

  time_t tstart = chrono::system_clock::to_time_t(start);
  tm * date = localtime(&tstart);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  auto midn = std::chrono::system_clock::from_time_t(std::mktime(date));

  auto hours = chrono::duration_cast<chrono::hours>(start-midn);
  auto minutes = chrono::duration_cast<chrono::minutes>(start-midn);

  auto start_ = chrono::duration_cast<chrono::microseconds>(start-midn-hours-minutes).count();
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    auto follower_id = p.first;

    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, start, ballot, leader_id, src_coroid, follower_id] (Future* fu) {
      ballot_t b = 0;
      uint64_t coro_id = 0;
      fu->get_reply() >> b >> coro_id;
      e->FeedResponse(b==ballot);
      auto end = chrono::system_clock::now();
      auto duration = chrono::duration_cast<chrono::microseconds>(end-start).count();
      //Log_info("The duration of Accept() for %d is: %d", follower_id, duration);
      // e->deps[leader_id][src_coroid][follower_id].erase(-1);
      // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
    };
    MarshallDeputy md(cmd);
    auto start1 = chrono::system_clock::now();
    auto f = proxy->async_Accept(slot_id, start_, ballot, md, fuattr);
    auto end1 = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end1-start1).count();
    // Log_info("Time for Async_Accept() for %d is: %d", follower_id, duration);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<PaxosAcceptQuorumEvent>
MultiPaxosCommo::CrpcAccept(parid_t par_id,
                                 siteid_t leader_site_id,
                                 slotid_t slot_id,
                                 ballot_t ballot,
                                 shared_ptr<Marshallable> cmd) {
  // Log_info("Tracepath vi");
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<PaxosAcceptQuorumEvent>(n, n/2+1);
  // auto e = Reactor::CreateSpEvent<PaxosAcceptQuorumEvent>(n, n);

  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first; // might need to be changed to coordinator's id
  vector<Future*> fus;
  auto start = chrono::system_clock::now();

  time_t tstart = chrono::system_clock::to_time_t(start);
  tm * date = localtime(&tstart);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  auto midn = std::chrono::system_clock::from_time_t(std::mktime(date));

  auto hours = chrono::duration_cast<chrono::hours>(start-midn);
  auto minutes = chrono::duration_cast<chrono::minutes>(start-midn);

  auto start_ = chrono::duration_cast<chrono::microseconds>(start-midn-hours-minutes).count();
  WAN_WAIT;
  std::vector<uint16_t> sitesInfo_; // additional; looks like can be computed in cRPC call

  for (auto& p : proxies) {
    auto id = p.first;
    auto proxy = (MultiPaxosProxy*) p.second;
    if (id != leader_site_id) { // #cPRC additional
      sitesInfo_.push_back(id); // #cPRC additional
    }                           // #cPRC additional
		//clients.push_back(cli);
  }

  sitesInfo_.push_back(leader_site_id); // #cPRC additional

  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    auto follower_id = p.first;
	  if (p.first == leader_site_id) {
        // fix the 1c1s1p bug
        // Log_info("leader_site_id %d", leader_site_id);
        e->FeedResponse(1);
        continue;
    }

    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    // FutureAttr fuattr;
    // fuattr.callback = [e, start, ballot, leader_id, src_coroid, follower_id] (Future* fu) {
    //   ballot_t b = 0;
    //   uint64_t coro_id = 0;
    //   fu->get_reply() >> b >> coro_id;
    //   e->FeedResponse(b==ballot);
    //   auto end = chrono::system_clock::now();
    //   auto duration = chrono::duration_cast<chrono::microseconds>(end-start).count();
    //   //Log_info("The duration of Accept() for %d is: %d", follower_id, duration);
    //   // e->deps[leader_id][src_coroid][follower_id].erase(-1);
    //   // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
    // };

    MarshallDeputy md(cmd);
    std::vector<PaxosMessage> state;

    // crpc_id generation is also not abstracted
    uint64_t crpc_id = reinterpret_cast<uint64_t>(&e);
    // // Log_info("*** crpc_id is: %d", crpc_id); // verify it's never the same
    verify(cRPCEvents.find(crpc_id) == cRPCEvents.end());

    auto start1 = chrono::system_clock::now();
    auto f = proxy->async_CrpcAccept(crpc_id, slot_id, start_, ballot, md, sitesInfo_, state);
    auto end1 = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end1-start1).count();
    // Log_info("Time for Async_Accept() for %d is: %d", follower_id, duration);
    Future::safe_release(f);

    // this too should be abstracted
    cRPCEvents[crpc_id] = e;

    // rather than breaking, do something else; when iterating through proxies
    break;
  }
  return e;
}

void MultiPaxosCommo::CrpcProxyAccept(const uint64_t& id,
                           const slotid_t slot_id,
		                       const uint64_t time,
                           const ballot_t ballot,
                           const MarshallDeputy& cmd,
                           const std::vector<uint16_t>& addrChain,
                           const vector<PaxosMessage> state) {

  // Log_info("Tracepath vii");
  auto proxy = (MultiPaxosProxy *)rpc_proxies_[addrChain[0]];
  // Log_info("Tracepath vii a");
  auto f = proxy->async_CrpcAccept(id, slot_id, time, ballot, cmd, addrChain, state);
  // Log_info("Tracepath vii b");
  Future::safe_release(f);
}

void MultiPaxosCommo::BroadcastAccept(parid_t par_id,
                                      slotid_t slot_id,
                                      ballot_t ballot,
                                      shared_ptr<Marshallable> cmd,
                                      const function<void(Future*)>& cb) {
  verify(0); // deprecated function
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  vector<Future*> fus;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = cb;
    MarshallDeputy md(cmd);
    uint64_t time = 0; // compiles the code
    auto f = proxy->async_Accept(slot_id, time,ballot, md, fuattr);
    Future::safe_release(f);
  }
//  verify(0);
}

void MultiPaxosCommo::BroadcastDecide(const parid_t par_id,
                                      const slotid_t slot_id,
                                      const ballot_t ballot,
                                      const shared_ptr<Marshallable> cmd) {
  // Log_info("Tracepath VIII");
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  vector<Future*> fus;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = [](Future* fu) {};
    MarshallDeputy md(cmd);
    auto f = proxy->async_Decide(slot_id, ballot, md, fuattr);
    //sp_quorum_event->add_dep(leader_id, p.first);
    Future::safe_release(f);
  }
}

} // namespace janus
