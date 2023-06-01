#!/usr/bin/env bash
set -x -e
g++ -c -I. -I ../src/rrr ex_proc.cc ex_svc.cc
g++ -c -I. -I ../src/rrr ex_clnt.cc
g++ ex_proc.o ex_svc.o -o rpc_svc -L ../build -lRRR -lMEMDB -lEXTERN_C -lpthread -L. -L ../src/rrr -lboost_coroutine -lboost_context -lboost_system
g++ ex_clnt.o -o rpc_clnt -L ../build -lRRR -lMEMDB -lEXTERN_C -lpthread -L. -L ../src/rrr -lboost_coroutine -lboost_context -lboost_system

./rpc_svc &
./rpc_clnt
sleep 2
./rpc_clnt
sleep 2
./rpc_clnt
sleep 2
