#include <peersafe/app/prometh/PrometheusClient.h>
#include <peersafe/app/ledger/LedgerAdjust.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <ripple/core/JobQueue.h>

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/main/Application.h>

#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/impl/AccountTxPaging.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/UintTypes.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <boost/format.hpp>
#include <memory>
#include <ripple/core/ConfigSections.h>
namespace ripple {

std::string
static getPort(Application& app)
{
    std::string port;
    auto prometh_section = app.config().section(ConfigSection::prometheus());
    if (prometh_section.empty())
    {
        return "";
    }
    std::pair<std::string, bool> portCfg = prometh_section.find("port");
    if (portCfg.second && !portCfg.first.empty())
        port = portCfg.first;
    else
        return "";
    return port;
}

PromethExposer::PromethExposer(
    Application& app,
    Config& cfg,
    std::string const& pubKey,
    beast::Journal journal)
    : app_(app)
    , journal_(journal)
    , cfg_(cfg)
    , pubkey_node_(pubKey)
    , exposer()
    , registry(std::make_shared<prometheus::Registry>())
   
{
     using namespace prometheus;

    try
    {
        std::string port;
        port = getPort(app);
        if (port.empty())
            return;
        auto address = "0.0.0.0:"+ port;
        exposer = std::make_unique<prometheus::Exposer>(address);
        exposer->RegisterCollectable(registry);
    }
    catch (std::exception const&)
    {
        // ignore any exceptions, so the command
        // line client works without a config file
        app_.logs().journal("PromethExposer").error();
    }   
    

}
PromethExposer::~PromethExposer()
{
}
std::shared_ptr<prometheus::Registry>&
PromethExposer::getRegistry()
{
    return registry;
}

std::string const&
PromethExposer::getPubKey()
{
    return pubkey_node_;
}

PrometheusClient::PrometheusClient(
    Schema& app,
    Config& cfg,
    PromethExposer& exposer,
    beast::Journal journal)
    : app_(app)
    , journal_(journal)
    , cfg_(cfg)
    , exposer_(exposer)
    , mPromethTime(app.timeKeeper().closeTime())
    , schema_gauge(prometheus::BuildGauge()
                             .Name("Chainsqld_schema_total")
                             .Help("Number of schema")
                             .Register(*exposer_.getRegistry())
                            .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"peer", exposer_.getPubKey()}}))
    , peer_gauge(prometheus::BuildGauge()
                            .Name("Chainsqld_peer_status")
                            .Help("peer status")
                            .Register(*exposer_.getRegistry())
                            .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}, {"peer_status", "peer_status"}}))
    , txSucessCount_gauge(prometheus::BuildGauge()
                                    .Name("Chainsqld_tx_success_count")
                                    .Help("tx success count")
                                    .Register(*exposer_.getRegistry())
                                    .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))
    , txFailCount_gauge(prometheus::BuildGauge()
                                  .Name("Chainsqld_tx_fail_count")
                                  .Help("tx fail count")
                                  .Register(*exposer_.getRegistry())
                                  .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))
    , contractCreateCount_gauge(prometheus::BuildGauge()
                                        .Name("Chainsqld_contract_create_count")
                                        .Help("contract create count")
                                        .Register(*exposer_.getRegistry())
                                        .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))
    , contractCallCount_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_contract_call_count")
                                        .Help("contract call count")
                                        .Register(*exposer_.getRegistry())
                                        .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))
    , accountCount_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_account_count")
                                   .Help("account count")
                                   .Register(*exposer_.getRegistry())
                                   .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))
    , blockHeight_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_block_height")
                                   .Help("block height")
                                   .Register(*exposer_.getRegistry())
                                   .Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())},{"pubkey_node", exposer_.getPubKey()}}))

{
    
}
PrometheusClient::~PrometheusClient()
{
}


void
PrometheusClient::timerEntry(NetClock::time_point const& now)
{
    std::string port = getPort(app_.app());
     
    if (port.empty() || now - mPromethTime < promethDataCollectionInterval)
    {
        return;
    }
    mPromethTime = now;
    int count = getSchemaCount(app_);
    schema_gauge.Set(count);

    auto server_status = app_.getOPs().getServerStatus();
    int status = server_status == "normal" ? 1 : 0;
    peer_gauge.Set(status);

    auto ledger = app_.openLedger().current();
    if (ledger != nullptr)
    {
        auto sleStatis = ledger->read(keylet::statis());
        if (sleStatis)
        {
            auto count = sleStatis->getFieldU32(sfTxSuccessCountField);
            txSucessCount_gauge.Set(count);
            count = sleStatis->getFieldU32(sfTxFailureCountField);
            txFailCount_gauge.Set(count);
            count = sleStatis->getFieldU32(sfContractCreateCountField);
            contractCreateCount_gauge.Set(count);
            count = sleStatis->getFieldU32(sfContractCallCountField);
            contractCallCount_gauge.Set(count);
            count = sleStatis->getFieldU32(sfAccountCountField);
            accountCount_gauge.Set(count);
        }
        //else
        //{
        //    TxnDBCon& connection = app_.getTxnDB();
        //    txSucessCount_gauge.Set( LedgerAdjust::getTxSucessCount(connection.checkoutDbRead()));
        //    txFailCount_gauge.Set(LedgerAdjust::getTxFailCount(connection.checkoutDbRead()));
        //    contractCallCount_gauge.Set(LedgerAdjust::getContractCallCount(connection.checkoutDbRead()));
        //   
        //    int contractCount = LedgerAdjust::getContractCreateCount(connection.checkoutDbRead());
        //    contractCreateCount_gauge.Set(contractCount);
        //    int accountCount = LedgerAdjust::getAccountCount(connection.checkoutDbRead());
        //    if (accountCount > 1)
        //        accountCount = accountCount - contractCount;
        //    accountCount_gauge.Set(accountCount);
        //}
        //blockHeight_gauge.Set(ledger->info().seq -1);
    }
    else
        blockHeight_gauge.Set(-1);
}

int
PrometheusClient::getSchemaCount(Schema& app)
{
    int schemaCount = 0;
    auto ledger = app.openLedger().current();
    if (ledger == nullptr)
        return schemaCount;

    // This is a time-consuming process for a project that has many sles.
    auto sleIndex = ledger->read(keylet::schema_index());
    if (!sleIndex)
        return schemaCount;
    else
    {
        auto& schemaIndexes = sleIndex->getFieldV256(sfSchemaIndexes);
        for (auto const& index : schemaIndexes)
        {
            auto key = Keylet(ltSCHEMA, index);
            auto sle = ledger->read(key);
            if (sle)
            {
                schemaCount++;
            }
        }
    }

    return schemaCount;
}

}

