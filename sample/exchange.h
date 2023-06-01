#pragma once

#include "rrr.hpp"

#include <errno.h>


namespace exchange {

struct msg {
    std::string from;
    std::string to;
    std::string msg;
};

inline rrr::Marshal& operator <<(rrr::Marshal& m, const msg& o) {
    m << o.from;
    m << o.to;
    m << o.msg;
    return m;
}

inline rrr::Marshal& operator >>(rrr::Marshal& m, msg& o) {
    m >> o.from;
    m >> o.to;
    m >> o.msg;
    return m;
}

class ExchangeService: public rrr::Service {
public:
    enum {
        CAST = 0x3700a295,
    };
    int __reg_to__(rrr::Server* svr) {
        int ret = 0;
        if ((ret = svr->reg(CAST, this, &ExchangeService::__cast__wrapper__)) != 0) {
            goto err;
        }
        return 0;
    err:
        svr->unreg(CAST);
        return ret;
    }
    // these RPC handler functions need to be implemented by user
    // for 'raw' handlers, remember to reply req, delete req, and sconn->release(); use sconn->run_async for heavy job
    virtual void cast(const msg& content, std::string* status, rrr::DeferredReply* defer) = 0;
private:
    void __cast__wrapper__(rrr::Request* req, rrr::ServerConnection* sconn) {
        msg* in_0 = new msg;
        req->m >> *in_0;
        std::string* out_0 = new std::string;
        auto __marshal_reply__ = [=] {
            *sconn << *out_0;
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete out_0;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(req, sconn, __marshal_reply__, __cleanup__);
        this->cast(*in_0, out_0, __defer__);
    }
};

class ExchangeProxy {
protected:
    rrr::Client* __cl__;
public:
    ExchangeProxy(rrr::Client* cl): __cl__(cl) { }
    rrr::Future* async_cast(const msg& content, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        rrr::Future* __fu__ = __cl__->begin_request(ExchangeService::CAST, __fu_attr__);
        if (__fu__ != nullptr) {
            *__cl__ << content;
        }
        __cl__->end_request();
        return __fu__;
    }
    rrr::i32 cast(const msg& content, std::string* status) {
        rrr::Future* __fu__ = this->async_cast(content);
        if (__fu__ == nullptr) {
            return ENOTCONN;
        }
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *status;
        }
        __fu__->release();
        return __ret__;
    }
};

} // namespace exchange



