#include "all.h"

namespace deptran {
  char RW_BENCHMARK_TABLE[] = "customer";

  void RWPiece::reg_all() {
      reg_pieces();
  }

  void RWPiece::reg_pieces() {
    BEGIN_PIE(RW_BENCHMARK_R_TXN, RW_BENCHMARK_R_TXN_0, DF_NO) {
      mdb::Txn *txn = dtxn->mdb_txn_;
      Value buf;
      verify(cmd.input.size() == 1);
      i32 output_index = 0;

      mdb::Row *r = txn->query(txn->get_table(RW_BENCHMARK_TABLE),
                               cmd.input.at(0)).next();
      if (!txn->read_column(r, 1, &buf)) {
          *res = REJECT;
          return;
      }
      output[output_index++] = buf;
      *res = SUCCESS;
      return;
    } END_PIE

    BEGIN_PIE(RW_BENCHMARK_W_TXN, RW_BENCHMARK_W_TXN_0, DF_REAL) {
      mdb::Txn *txn = dtxn->mdb_txn_;
      verify(cmd.input.size() == 1);
      i32 output_index = 0;
      Value buf;
      mdb::Row *r = txn->query(txn->get_table(RW_BENCHMARK_TABLE),
                               cmd.input[0]).next();
      if (!txn->read_column(r, 1, &buf)) {
        *res = REJECT;
        return;
      }
      buf.set_i64(buf.get_i64() + 1);
      if (!txn->write_column(r, 1, /*input[1]*/buf)) {
        *res = REJECT;
        return;
      }
      *res = SUCCESS;
      return;
    } END_PIE
  }
}

