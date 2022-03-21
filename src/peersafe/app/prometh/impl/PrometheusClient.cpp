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
PromethExposer::getPort(Application& app)
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
    , m_exposer()
    , m_registry(std::make_shared<prometheus::Registry>())
    , m_schema_gauge(prometheus::BuildGauge()
                             .Name("Chainsqld_schema_total")
                             .Help("Number of schema")
                             .Labels({{"pubkey_node", pubkey_node_}})
                             .Register(*m_registry))
    , m_peer_gauge(prometheus::BuildGauge()
                            .Name("Chainsqld_peer_status")
                            .Help("peer status")
                            .Labels({{"pubkey_node", pubkey_node_}})
                            .Register(*m_registry))
    , m_txSucessCount_gauge(prometheus::BuildGauge()
                                    .Name("Chainsqld_tx_success_count")
                                    .Help("tx success count")
                                    .Labels({{"pubkey_node", pubkey_node_}})
                                    .Register(*m_registry))
    , m_txFailCount_gauge(prometheus::BuildGauge()
                                  .Name("Chainsqld_tx_fail_count")
                                  .Help("tx fail count")
                                  .Labels({{"pubkey_node", pubkey_node_}})
                                  .Register(*m_registry))
    , m_contractCreateCount_gauge(prometheus::BuildGauge()
                                        .Name("Chainsqld_contract_create_count")
                                        .Help("contract create count")
                                        .Labels({{"pubkey_node", pubkey_node_}})
                                        .Register(*m_registry))
    , m_contractCallCount_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_contract_call_count")
                                        .Help("contract call count")
                                        .Labels({{"pubkey_node", pubkey_node_}})
                                        .Register(*m_registry))
    , m_accountCount_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_account_count")
                                   .Help("account count")
                                   .Labels({{"pubkey_node", pubkey_node_}})
                                   .Register(*m_registry))
    , m_blockHeight_gauge(prometheus::BuildGauge()
                                   .Name("Chainsqld_block_height")
                                   .Help("block height")
                                   .Labels({{"pubkey_node", pubkey_node_}})
                                   .Register(*m_registry))
{
     using namespace prometheus;

    try
    {
        std::string port;
        port = getPort(app);
        if (port.empty())
            return;
        auto address = "0.0.0.0:"+ port;
        m_exposer = std::make_unique<prometheus::Exposer>(address);
        m_exposer->RegisterCollectable(m_registry);
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
    m_registry.reset();

}
std::shared_ptr<prometheus::Registry>
PromethExposer::getRegistry()
{
        return m_registry;
}

std::string const&
PromethExposer::getPubKey()
{
    return pubkey_node_;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getSchemaGauge()
{
    return m_schema_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getPeerGauge()
{
    return m_peer_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getTxSucessCountGauge()
{
    return m_txSucessCount_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getTxFailCountGauge()
{
    return m_txFailCount_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getContractCreateCountGauge()
{
    return m_contractCreateCount_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getContractCallCountGauge()
{
    return m_contractCallCount_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getAccountCountGauge()
{
    return m_accountCount_gauge;
}

prometheus::Family<prometheus::Gauge>&
PromethExposer::getBlockHeightGauge()
{
    return m_blockHeight_gauge;
}

PrometheusClient::PrometheusClient(
    Schema& app,
    Config& cfg,
    PromethExposer& exposer,
    beast::Journal journal)
    : app_(app)
    , journal_(journal)
    , cfg_(cfg)
    , m_promethTime(app.timeKeeper().closeTime())
    , exposer_(exposer)
    , m_schema_gauge(exposer_.getSchemaGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_peer_gauge(exposer_.getPeerGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}, {"peer_status", "peer_status"}}))
    , m_txSucessCount_gauge(exposer_.getTxSucessCountGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_txFailCount_gauge(exposer_.getTxFailCountGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_contractCreateCount_gauge(exposer_.getContractCreateCountGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_contractCallCount_gauge(exposer_.getContractCallCountGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_accountCount_gauge(exposer_.getAccountCountGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_blockHeight_gauge(exposer_.getBlockHeightGauge().Add({{"schemaId", to_string(app_.getSchemaParams().schemaId())}}))
    , m_promethSle(std::make_unique<SLE>(keylet::statis()))
    
{
    
}
PrometheusClient::~PrometheusClient()
{
    exposer_.getSchemaGauge().Remove(&m_schema_gauge);
    exposer_.getPeerGauge().Remove(&m_peer_gauge);
    exposer_.getTxSucessCountGauge().Remove(&m_txSucessCount_gauge);
    exposer_.getTxFailCountGauge().Remove(&m_txFailCount_gauge);
    exposer_.getContractCreateCountGauge().Remove(&m_contractCreateCount_gauge);
    exposer_.getContractCallCountGauge().Remove(&m_contractCallCount_gauge);
    exposer_.getAccountCountGauge().Remove(&m_accountCount_gauge);
    exposer_.getBlockHeightGauge().Remove(&m_blockHeight_gauge);
}

std::shared_ptr<SLE>&
PrometheusClient::getPromethSle()
{
    return m_promethSle;
}

void 
PrometheusClient::setup()
{
    if (exposer_.getPort(app_.app()).empty())
        return;
    auto ledger = app_.openLedger().current();
    if (ledger != nullptr)
    {
        auto sleStatis = ledger->read(keylet::statis());
        if (sleStatis)
        {
            return;
        }
    }
    app_.getJobQueue().addJob(jtCREATE_PROMETH_SLE, "CreatePromethSle", [this](Job&) { LedgerAdjust::createSle(app_); },app_.doJobCounter());
}

void
PrometheusClient::timerEntry(NetClock::time_point const& now)
{
    std::string port = app_.app().getPromethExposer().getPort(app_.app());
     
    if (port.empty() || now - m_promethTime < promethDataCollectionInterval)
    {
        return;
    }
    m_promethTime = now;
    int count = getSchemaCount(app_);
    m_schema_gauge.Set(count);

    auto server_status = app_.getOPs().getServerStatus();
    int status = server_status == "normal" ? 1 : 0;
    m_peer_gauge.Set(status);

    auto ledger = app_.openLedger().current();
    if (ledger != nullptr)
    {
        auto sleStatis = ledger->read(keylet::statis());
        if (!sleStatis)
        {
            if (LedgerAdjust::isCompleteReadData(app_))
                sleStatis = getPromethSle();
            else
                return;
        }
       
        auto count = sleStatis->getFieldU32(sfTxSuccessCountField);
        m_txSucessCount_gauge.Set(count);
        count = sleStatis->getFieldU32(sfTxFailureCountField);
        m_txFailCount_gauge.Set(count);
        count = sleStatis->getFieldU32(sfContractCreateCountField);
        m_contractCreateCount_gauge.Set(count);
        count = sleStatis->getFieldU32(sfContractCallCountField);
        m_contractCallCount_gauge.Set(count);
        count = sleStatis->getFieldU32(sfAccountCountField);
        m_accountCount_gauge.Set(count);
        m_blockHeight_gauge.Set(ledger->info().seq -1);
    }
    else
        m_blockHeight_gauge.Set(-1);
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

