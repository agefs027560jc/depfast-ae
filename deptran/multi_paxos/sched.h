#pragma once

#include "__dep__.h"
#include "constants.h"
#include "scheduler.h"

namespace rococo {

class Command;
class MultiPaxosSched : public Scheduler {
 public:
  void OnPrepareRequest(slotid_t slot_id,
                       ballot_t ballot,
                       ballot_t* max_ballot,
                       const function<void()>& cb);

  void OnAcceptRequest(const slotid_t slot_id,
                       const ballot_t ballot,
                       const Command& cmd,
                       ballot_t* max_ballot,
                       const function<void()>& cb);

  void OnDecideRequest(const slotid_t slot_id,
                       const ballot_t ballot,
                       const Command& cmd);
};

} // namespace rococo