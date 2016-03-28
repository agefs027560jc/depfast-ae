
/**
 * What shoud we do to change this to asynchronous?
 * 1. Fisrt we need to have a queue to hold all transaction requests.
 * 2. pop next request, send start request for each piece until there is no
 *available left.
 *          in the callback, send the next piece of start request.
 *          if responses to all start requests are collected.
 *              send the finish request
 *                  in callback, remove it from queue.
 *
 *
 */

#include "coord.h"
#include "../frame.h"
#include "../benchmark_control_rpc.h"

namespace rococo {

ThreePhaseCoordinator::ThreePhaseCoordinator(uint32_t coo_id,
                                             int benchmark,
                                             ClientControlServiceImpl *ccsi,
                                             uint32_t thread_id)
    : Coordinator(coo_id,
                  benchmark,
                  ccsi,
                  thread_id) {
  // TODO: doesn't belong here;
  // it is currently here so that subclasses such as RCCCoord and OCCoord don't break
  verify(commo_ == nullptr);
  commo_ = new RococoCommunicator();
}


/** thread safe */
void ThreePhaseCoordinator::do_one(TxnRequest &req) {
  // pre-process
  std::lock_guard<std::recursive_mutex> lock(this->mtx_);
  TxnCommand *cmd = frame_->CreateTxnCommand(req, txn_reg_);
  verify(txn_reg_ != nullptr);
  cmd_ = cmd;
  cmd_->root_id_ = this->next_txn_id();
  cmd_->id_ = cmd_->root_id_;
  Reset(); // In case of reuse.

  Log_debug("do one request txn_id: %ld\n", cmd_->id_);

  if (ccsi_) ccsi_->txn_start_one(thread_id_, cmd->type_);

  auto mode = Config::GetConfig()->cc_mode_;
  switch (mode) {
    case MODE_OCC:
    case MODE_2PL:
      Dispatch();
      break;
    case MODE_RPC_NULL:
//      rpc_null_start(ch);
//      break;
    case MODE_RCC:
    case MODE_RO6:
    case MODE_NONE:
    default:
      verify(0);
  }
  // finish request is triggered in the callback of start request.
}

//void ThreePhaseCoord::rpc_null_start(TxnChopper *ch) {
//  rrr::FutureAttr fuattr;
//
//  fuattr.callback = [ch, this](Future *fu) {
//    ch->reply_.res_ = SUCCESS;
//    TxnReply &txn_reply_buf = ch->get_reply();
//    double last_latency = ch->last_attempt_latency();
//    this->report(txn_reply_buf, last_latency
//#ifdef TXN_STAT
//        , ch
//#endif // ifdef TXN_STAT
//    );
//    ch->callback_(txn_reply_buf);
//    delete ch;
//  };
//
//  RococoProxy *proxy = commo_->vec_rpc_proxy_[coo_id_ % commo_->vec_rpc_proxy_.size()];
//  Future::safe_release(proxy->async_rpc_null(fuattr));
//}

void ThreePhaseCoordinator::IncrementPhaseAndChangeStage(CoordinatorStage stage) {
  phase_++;
  stage_ = stage;
  Log_debug("moving to phase %ld; stage %d", phase_, stage_);
}

bool ThreePhaseCoordinator::IsPhaseOrStageStale(phase_t phase, CoordinatorStage stage) {
  bool result = false;
  if (phase_ != phase) {
    Log_debug("phase %d doesn't match %d\n", phase, phase_);
    result = true;
  }
  if (stage_ != stage) {
    Log_debug("stage %d doesn't match %d\n", stage, stage_);
    result = true;
  }
  return result;
}

void ThreePhaseCoordinator::Reset() {
  for (int i = 0; i < site_prepare_.size(); i++) {
    site_prepare_[i] = 0;
  }
  n_handout_ = 0;
  n_handout_ack_ = 0;
  n_prepare_req_ = 0;
  n_prepare_ack_ = 0;
  n_finish_req_ = 0;
  n_finish_ack_ = 0;
  handout_acks_.clear();
  IncrementPhaseAndChangeStage(HANDOUT);
//  TxnCommand *ch = (TxnCommand *) cmd_;
}

void ThreePhaseCoordinator::restart(TxnCommand *ch) {
  std::lock_guard<std::recursive_mutex> lock(this->mtx_);
  Reset();
  ch->retry();

  double last_latency = ch->last_attempt_latency();

  if (ccsi_) ccsi_->txn_retry_one(this->thread_id_, ch->type_, last_latency);
  Dispatch();
}

void ThreePhaseCoordinator::Dispatch() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  //  ___TestPhaseOne(cmd_id_);

  if (stage_ != HANDOUT) {
    IncrementPhaseAndChangeStage(HANDOUT);
  }

//  StartRequest req;
//  req.cmd_id = cmd_id_;

  int cnt = 0;
  while (cmd_->HasMoreSubCmdReadyNotOut()) {
    auto subcmd = (SimpleCommand*) cmd_->GetNextReadySubCmd();
    subcmd->id_ = next_pie_id();
    verify(subcmd->root_id_ == cmd_->id_);
    n_handout_++;
    cnt++;
    Log_debug("send out start request %ld, cmd_id: %lx, inn_id: %d, pie_id: %lx",
              n_handout_, cmd_->id_, subcmd->inn_id_, subcmd->id_);
    handout_acks_[subcmd->inn_id()] = false;
    commo()->SendHandout(*subcmd,
                         this,
                         std::bind(&ThreePhaseCoordinator::HandoutAck,
                                   this,
                                   phase_,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
  }
  Log_debug("sent %d SubCmds\n", cnt);
}

bool ThreePhaseCoordinator::AllHandoutAckReceived() {
  return std::all_of(handout_acks_.begin(),
                     handout_acks_.end(),
                     [](std::pair<innid_t, bool> pair){ return pair.second; });
}

void ThreePhaseCoordinator::HandoutAck(phase_t phase, int res, Command& cmd) {
  std::lock_guard<std::recursive_mutex> lock(this->mtx_);

  if (IsPhaseOrStageStale(phase, HANDOUT)) {
    Log_debug("ignore stale startack\n");
    return;
  }
  n_handout_ack_++;
  TxnCommand *ch = (TxnCommand *) cmd_;
  handout_acks_[cmd.inn_id_] = true;

  Log_debug("get start ack %ld/%ld for cmd_id: %lx, inn_id: %d",
            n_handout_ack_, n_handout_, cmd_->id_, cmd.inn_id_);

  if (res == REJECT) {
    Log_debug("got REJECT reply for cmd_id: %lx, inn_id: %d; NOT COMMITING",
               cmd.root_id_, cmd.inn_id());
    ch->commit_.store(false);
  }
  if (!ch->commit_.load()) {
    if (n_handout_ack_ == n_handout_) {
      Log_debug("received all start acks (at least one is REJECT); calling "
                    "Finish()");
      this->Decide();
    }
  } else {
    cmd_->Merge(cmd);
    if (cmd_->HasMoreSubCmdReadyNotOut()) {
      Log_debug("command has more sub-cmd, cmd_id: %lx,"
                    " n_started_: %d, n_pieces: %d",
                cmd_->id_, ch->n_pieces_out_, ch->GetNPieceAll());
      Dispatch();
    } else if (AllHandoutAckReceived()) {
      Log_debug("receive all start acks, txn_id: %ld; START PREPARE", cmd_->id_);
      Prepare();
    }
  }
}

/** caller should be thread_safe */
void ThreePhaseCoordinator::Prepare() {
  IncrementPhaseAndChangeStage(PREPARE);
  TxnCommand *cmd = (TxnCommand *) cmd_;
  auto mode = Config::GetConfig()->cc_mode_;
  verify(mode == MODE_OCC || mode == MODE_2PL);

  std::vector<i32> sids;
  for (auto &site : cmd->partition_ids_) {
    sids.push_back(site);
  }

  for (auto &partition_id : cmd->partition_ids_) {
    Log_debug("send prepare tid: %ld; site %d", cmd_->id_, partition_id);
    commo()->SendPrepare(partition_id,
                         cmd_->id_,
                         sids,
                         std::bind(&ThreePhaseCoordinator::PrepareAck,
                                  this,
                                  phase_,
                                  std::placeholders::_1));
    verify(site_prepare_[partition_id] == 0);
    site_prepare_[partition_id]++;
    verify(site_prepare_[partition_id] == 1);
  }
}

void ThreePhaseCoordinator::PrepareAck(phase_t phase, Future *fu) {
  std::lock_guard<std::recursive_mutex> lock(this->mtx_);
  if (IsPhaseOrStageStale(phase, PREPARE)) {
    Log_debug("ignore stale prepareack\n");
    return;
  }
  TxnCommand* cmd = (TxnCommand*) cmd_;
  n_prepare_ack_++;
  int32_t e = fu->get_error_code();

  if (e != 0) {
    cmd->commit_.store(false);
    Log_fatal("2PL prepare failed due to error");
  }

  int res;
  fu->get_reply() >> res;
  Log_debug("tid %ld; prepare result %d", cmd_->root_id_, res);

  if (res == REJECT) {
    Log_debug("Prepare rejected for %ld by %ld\n",
               cmd_->root_id_,
               cmd->inn_id());
    cmd->commit_.store(false);
  }

  if (n_prepare_ack_ == cmd->partition_ids_.size()) {
    Log_debug("2PL prepare finished for %ld", cmd->root_id_);
    this->Decide();
  } else {
    // Do nothing.
  }
}

void ThreePhaseCoordinator::Decide() {
  std::lock_guard<std::recursive_mutex> lock(this->mtx_);
  IncrementPhaseAndChangeStage(FINISH);
  ___TestPhaseThree(cmd_->id_);
  TxnCommand *cmd = (TxnCommand *) cmd_;
  auto mode = Config::GetConfig()->cc_mode_;
  verify(mode == MODE_OCC || mode == MODE_2PL);
  Log_debug("send out finish request, cmd_id: %lx, %ld", cmd->id_, n_finish_req_);

  // commit or abort piece
//  RequestHeader header = gen_header(cmd);
  if (cmd->commit_.load()) {
    cmd->reply_.res_ = SUCCESS;
    for (auto &rp : cmd->partition_ids_) {
      n_finish_req_ ++;
      Log_debug("send commit for txn_id %ld to %ld\n", cmd->id_, rp);
      commo()->SendCommit(rp,
                         cmd_->id_,
                         std::bind(&ThreePhaseCoordinator::FinishAck,
                                   this,
                                   phase_,
                                   std::placeholders::_1));
      site_commit_[rp]++;
    }
  } else {
    cmd->reply_.res_ = REJECT;
    for (auto &rp : cmd->partition_ids_) {
      n_finish_req_ ++;
      Log_debug("send abort for txn_id %ld to %ld\n", cmd->id_, rp);
      commo()->SendAbort(rp,
                        cmd_->id_,
                        std::bind(&ThreePhaseCoordinator::FinishAck,
                                  this,
                                  phase_,
                                  std::placeholders::_1));
      site_abort_[rp]++;
    }
  }
}

void ThreePhaseCoordinator::FinishAck(phase_t phase, Future *fu) {
  TxnCommand* cmd = (TxnCommand*)cmd_;
  bool callback = false;
  bool retry = false;
  {
    std::lock_guard<std::recursive_mutex> lock(this->mtx_);
    if (IsPhaseOrStageStale(phase, FINISH)) {
      Log_debug("ignore stale finish ack\n");
      return;
    }

    n_finish_ack_++;
    Log_debug("finish cmd_id_: %ld; n_finish_ack_: %ld; n_finish_req_: %ld",
               cmd_->id_, n_finish_ack_, n_finish_req_);
    verify(cmd->GetPartitionIds().size() == n_finish_req_);
    if (n_finish_ack_ == cmd->GetPartitionIds().size()) {
      if ((cmd->reply_.res_ == REJECT) && cmd->can_retry()) {
        retry = true;
      } else {
        callback = true;
      }
    }
  }
  Log_debug("callback: %s, retry: %s",
             callback ? "True" : "False",
             retry ? "True" : "False");

  if (retry) {
    this->restart(cmd);
  }

  if (callback) {
    // generate a reply and callback.
    TxnReply &txn_reply_buf = cmd->get_reply();
    double last_latency = cmd->last_attempt_latency();
    this->report(txn_reply_buf, last_latency
#ifdef TXN_STAT
        , ch
#endif // ifdef TXN_STAT
    );

    cmd->callback_(txn_reply_buf);
    delete cmd;
  }
}

void ThreePhaseCoordinator::report(TxnReply &txn_reply,
                                   double last_latency
#ifdef TXN_STAT
    , TxnChopper *ch
#endif // ifdef TXN_STAT
) {
  if (ccsi_) {
    if (txn_reply.res_ == SUCCESS) {
#ifdef TXN_STAT
      txn_stats_[ch->txn_type_].one(ch->proxies_.size(), ch->p_types_);
#endif // ifdef TXN_STAT
      ccsi_->txn_success_one(thread_id_,
                             txn_reply.txn_type_,
                             txn_reply.start_time_,
                             txn_reply.time_,
                             last_latency,
                             txn_reply.n_try_);
    }
    else
      ccsi_->txn_reject_one(thread_id_,
                            txn_reply.txn_type_,
                            txn_reply.start_time_,
                            txn_reply.time_,
                            last_latency,
                            txn_reply.n_try_);
  }
}

void ThreePhaseCoordinator::___TestPhaseThree(txnid_t txn_id) {
  auto it = ___phase_three_tids_.find(txn_id);
//  verify(it == ___phase_three_tids_.end());
  ___phase_three_tids_.insert(txn_id);
}

void ThreePhaseCoordinator::___TestPhaseOne(txnid_t txn_id) {
  auto it = ___phase_one_tids_.find(txn_id);
  verify(it == ___phase_one_tids_.end());
  ___phase_one_tids_.insert(txn_id);
}


} // namespace rococo
