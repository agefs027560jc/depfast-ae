namespace cpp janus

service FpgaRaft {
  void Heartbeat(1: i64 leaderPrevLogIndex,
                 2: DepId dep_id,
                 3: i64 followerPrevLogIndex);

  void Forward(1: string cmd,
               2: i64 cmt_idx);

  void Vote(1: i64 lst_log_idx,
            2: string lst_log_term,
            3: string par_id,
            4: string cur_term,
            5: string max_ballot,
            6: bool vote_granted);

  void Vote2FPGA(1: i64 lst_log_idx,
                 2: string lst_log_term,
                 3: string par_id,
                 4: string cur_term,
                 5: string max_ballot,
                 6: bool vote_granted);

  void AppendEntries(1: i64 slot,
                     2: string ballot,
                     3: i64 leaderCurrentTerm,
                     4: i64 leaderPrevLogIndex,
                     5: i64 leaderPrevLogTerm,
                     6: i64 leaderCommitIndex,
                     7: DepId dep_id,
                     8: string cmd,
                     9: i64 followerAppendOK,
                     10: i64 followerCurrentTerm,
                     11: i64 followerLastLogIndex);
 
  void AppendEntries2(1: i64 slot,
                      2: string ballot,
                      3: i64 leaderCurrentTerm,
                      4: i64 leaderPrevLogIndex,
                      5: i64 leaderPrevLogTerm,
                      6: i64 leaderCommitIndex,
                      7: DepId dep_id,
                      8: string cmd,
                      9: i64 followerAppendOK,
                      10: i64 followerCurrentTerm,
                      11: i64 followerLastLogIndex);

  void Decide(1: i64 slot,
              2: string ballot,
              3: DepId dep_id,
              4: string cmd);
}

// Below is for statistics

struct DepId {
    1: string str;
    2: i64 id;
}

/*

struct ValueTimesPair {
    1: i64 value;
    2: i64 times;
}

struct TxnInfoRes {
    1: i32 start_txn;  // total number of started txns
    2: i32 total_txn;  // total number of finished txns
    3: i32 total_try;  // total number of tries finished
    4: i32 commit_txn; // number of commit transactions
    5: i32 num_exhausted; // number of txns that reached the retry limit
    6: list<double> this_latency; // latencies started && finish in this period
    7: list<double> last_latency; // latencies started in last period, finish in this period
    8: list<double> attempt_latency; // interval latencies for each attempts
    9: list<double> interval_latency; // latencies finish in this period
    10: list<double> all_interval_latency; // latencies finish in this period include txn's with exhausted retries
    11: list<i32> num_try;
}

struct ServerResponse {
    1: map<string, ValueTimesPair> statistics;
    2: double cpu_util;
    3: i64 r_cnt_sum;
    4: i64 r_cnt_num;
    5: i64 r_sz_sum;
    6: i64 r_sz_num;
}

struct ClientResponse  {
    1: map<i32, TxnInfoRes> txn_info; // statistics for each txn
    2: i64 run_sec;    // running time in seconds
    3: i64 run_nsec;   // running time in nano seconds
    4: i64 period_sec;    // running time in seconds
    5: i64 period_nsec;   // running time in nano seconds
    6: i32 is_finish;  // if client finishs
    7: i64 n_asking;   // asking finish request count
}

struct Profiling {
    1: double cpu_util;
    2: double tx_util;
    3: double rx_util;
    4: double mem_util;
}

struct TxDispatchRequest {
    1: i32 id;
    2: i32 tx_type;
    3: list<Value> input;
}

struct TxnDispatchResponse {

}

*/
