#include "exchange.h"

namespace exchange {

/*

void cast(const msg& content, std::string* status) {
    *status = "ret";
    if(content.from=="a" && content.to=="b")
      *status = "ack";
}



void ExchangeServiceImpl::cast(const msg& content, std::string* status) {
    *status = "ret";
    if(content.from=="a" && content.to=="b")
      *status = "ack";
}

*/

  class ExchangeServiceImpl : public ExchangeService {
    public :
      void cast(const msg& content, std::string* status, rrr::DeferredReply* defer);
  };

}
