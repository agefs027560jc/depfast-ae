#pragma once

#include "__dep__.h"
#include "constants.h"
#include "graph.h"
#include "graph_marshaler.h"
#include "rcc_rpc.h"

namespace rococo {

class Commo {
 public:
  rrr::PollMgr *rpc_poll_;
  std::vector<rrr::Client *> vec_rpc_cli_;
  std::vector<RococoProxy *> vec_rpc_proxy_;

  Commo(std::vector<std::string> &addrs);
  void SendStart(groupid_t gid,
                 RequestHeader &header,
                 std::vector<Value> &input,
                 int32_t output_size,
                 std::function<void(Future *fu)> &callback);
  void SendPrepare(groupid_t gid, txnid_t tid, std::vector<int32_t> &sids, std::function<void(Future *fu)> &callback);
};
} // namespace rococo