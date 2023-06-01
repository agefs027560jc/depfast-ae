#include "ex_impl.h"

/*
int main() {
    rpc::ClientPool clnt_pool;
    exchange::ExchangeProxy clnt(clnt_pool.get_client("127.0.0.1:8888"));
    //BenchmarkProxy* clnt = new BenchmarkProxy(clnt_pool->get_client(svc_addr));
    //rpc::PollMgr* poll = new rpc::PollMgr;
    //rpc::ClientPool* clnt_pool = new rpc::ClientPool(poll);
    //exchange::ExchangeProxy* clnt = new exchange::ExchangeProxy(clnt_pool->get_client("127.0.0.1:8888"));

    std::string who = "a";
    exchange::msg a;
    a.from = who;
    a.to = "b";
    a.msg = "syn";
    std::string ret;
    clnt.cast(a, &ret);

    if(ret == "ack" ) return 0;
    return 1;
}
*/

int main() {
    rrr::PollMgr *pm = new rrr::PollMgr();
    std::shared_ptr<rrr::Client> client = std::make_shared<rrr::Client>(pm);
    while (client->connect(std::string("127.0.0.1:8888").c_str())!=0) {
        usleep(100 * 1000); // retry to connect
    }
    exchange::ExchangeProxy *client_proxy = new exchange::ExchangeProxy(client.get());

    std::string who = "a";
    exchange::msg a;
    a.from = who;
    a.to = "b";
    a.msg = "syn";
    std::string ret;
    client_proxy->cast(a, &ret);

    if(ret == "ack" ) return 0;
    return 1;
}
