

#include "sched.h"
#include "../rcc/dtxn.h"

using namespace rococo;

void BrqSched::OnPreAccept(const txnid_t txn_id,
                           const vector<SimpleCommand>& cmds,
                           const RccGraph& graph,
                           int32_t* res,
                           RccGraph* res_graph,
                           function<void()> callback) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  if (RandomGenerator::rand(1, 2000) <= 1)
    Log_info("on pre-accept graph size: %d", graph.size());
  verify(txn_id > 0);
  dep_graph_->Aggregate(const_cast<RccGraph&>(graph));
  // TODO FIXME
  // add interference based on cmds.
  RccDTxn *dtxn = (RccDTxn *) GetOrCreateDTxn(cmds[0].root_id_);
  if (dtxn->tv_ == nullptr) {
    dep_graph_->FindOrCreateTxnInfo(cmds[0].root_id_, &dtxn->tv_);
  }
  for (auto& c: cmds) {
    map<int32_t, Value> output;
    dtxn->DispatchExecute(c, res, &output);
  }
  dep_graph_->MinItfrGraph(txn_id, res_graph);
  *res = SUCCESS;
  callback();
}

void BrqSched::OnCommit(const txnid_t cmd_id,
                        const RccGraph& graph,
                        int32_t* res,
                        TxnOutput* output,
                        const function<void()>& callback) {
  // TODO to support cascade abort
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  if (RandomGenerator::rand(1, 2000) <= 1)
    Log_info("on commit graph size: %d", graph.size());
  *res = SUCCESS;
  // union the graph into dep graph
  RccDTxn *dtxn = (RccDTxn*) GetDTxn(cmd_id);
  verify(dtxn != nullptr);

  auto v = dep_graph_->FindV(cmd_id);
  verify(v != nullptr);
  TxnInfo& info = *v->data_;

  verify(dtxn->ptr_output_repy_ == nullptr);
  dtxn->ptr_output_repy_ = output;


  if (info.IsExecuted()) {
    *res = SUCCESS;
    callback();
  } else if (info.IsAborted()) {
    *res = REJECT;
    callback();
  } else {
    dtxn->commit_request_received_ = true;
    dtxn->finish_reply_callback_ = [callback, res] (int r) {
      *res = r;
      callback();
    };
    // fast path without check wait list?
    if (graph.size() == 1) {
      auto v = dep_graph_->FindV(cmd_id);
      if (v->incoming_.size() == 0);
      Execute(*v->data_);
      return;
    } else {
      Log_debug("graph size on commit, %d", (int) graph.size());
//    verify(0);
    }
    dep_graph_->Aggregate(const_cast<RccGraph&>(graph));
    for (auto& pair: graph.vertex_index_) {
      // TODO optimize here.
      auto txnid = pair.first;
      auto v = dep_graph_->FindV(txnid);
      verify(v != nullptr);
      waitlist_.push_back(v);
    }
    CheckWaitlist();
  }
}

int BrqSched::OnInquire(cmdid_t cmd_id,
                        RccGraph *graph,
                        const function<void()> &callback) {
  RccDTxn *dtxn = (RccDTxn *) GetOrCreateDTxn(cmd_id);
  dep_graph_->FindOrCreateTxnInfo(cmd_id, &dtxn->tv_);
  RccVertex* v = dtxn->tv_;
  verify(v != nullptr);
  TxnInfo& info = *v->data_;
  //register an event, triggered when the status >= COMMITTING;
  verify (info.Involve(partition_id_));

  if (info.IsCommitting()) {
    dep_graph_->MinItfrGraph(cmd_id, graph);
    callback();
  } else {
    info.graphs_for_inquire_.push_back(graph);
    info.callbacks_for_inquire_.push_back(callback);
    verify(info.graphs_for_inquire_.size() ==
        info.callbacks_for_inquire_.size());
  }

}