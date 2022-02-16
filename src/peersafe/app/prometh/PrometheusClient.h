#ifndef RIPPLE_RPC_PROMETHEUS_CLIENT_UTIL_H_INCLUDED
#define RIPPLE_RPC_PROMETHEUS_CLIENT_UTIL_H_INCLUDED

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <ripple/core/Config.h>
#include <ripple/beast/utility/PropertyStream.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/TaggedCache.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/ledger/ApplyView.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

namespace ripple {
	//prometheus sync tool class
	
	class PromethExposer {

	public:
		PromethExposer(Application& app, Config& cfg, std::string const& pubKey, beast::Journal j);
		virtual ~PromethExposer();
        std::shared_ptr<prometheus::Registry>  getRegistry();
		std::string const& getPubKey();
		prometheus::Family<prometheus::Gauge>& getSchemaGauge();
        prometheus::Family<prometheus::Gauge>& getPeerGauge();
		prometheus::Family<prometheus::Gauge>& getTxSucessCountGauge();
		prometheus::Family<prometheus::Gauge>& getTxFailCountGauge();
		prometheus::Family<prometheus::Gauge>& getContractCreateCountGauge();
		prometheus::Family<prometheus::Gauge>& getContractCallCountGauge();
		prometheus::Family<prometheus::Gauge>& getAccountCountGauge();
		prometheus::Family<prometheus::Gauge>& getBlockHeightGauge();
	private:
		Application&			app_;
		beast::Journal          journal_;
		Config&                 cfg_;
		
        std::string pubkey_node_;
		
		std::shared_ptr<prometheus::Exposer>  exposer;
        std::shared_ptr<prometheus::Registry> registry;
        prometheus::Family<prometheus::Gauge>& schema_gauge;
		prometheus::Family<prometheus::Gauge>& peer_gauge;
		prometheus::Family<prometheus::Gauge>& txSucessCount_gauge;
		prometheus::Family<prometheus::Gauge>& txFailCount_gauge;
		prometheus::Family<prometheus::Gauge>& contractCreateCount_gauge;
		prometheus::Family<prometheus::Gauge>& contractCallCount_gauge;
		prometheus::Family<prometheus::Gauge>& accountCount_gauge;
		prometheus::Family<prometheus::Gauge>& blockHeight_gauge;
		
	};
	class PrometheusClient {

	public:
            PrometheusClient(
                Schema& app,
                Config& cfg,
                PromethExposer& exposer,
                beast::Journal journal);
		
		virtual ~PrometheusClient();
		void timerEntry(NetClock::time_point const& now);
        int getSchemaCount(Schema& app);
       
	private:
		Schema&				app_;
		beast::Journal      journal_;
		Config&             cfg_;

		NetClock::time_point mPromethTime;
		PromethExposer&  exposer_;
		prometheus::Gauge& schema_gauge;
		prometheus::Gauge& peer_gauge;
		prometheus::Gauge& txSucessCount_gauge;
		prometheus::Gauge& txFailCount_gauge;
		prometheus::Gauge& contractCreateCount_gauge;
		prometheus::Gauge& contractCallCount_gauge;
		prometheus::Gauge& accountCount_gauge;
		prometheus::Gauge& blockHeight_gauge;
        
		
	};
	 static std::chrono::seconds const promethDataCollectionInterval(5);
}

#endif
