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
#include <peersafe/app/ledger/LedgerAdjust.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <boost/format.hpp>

namespace ripple {
int 
LedgerAdjust::getTxSucessCount(LockedSociSession db)
{
    boost::optional<int> value;
    int count = 0;
    std::string sql = boost::str(boost::format(
                    R"(select count(*) from Transactions WHERE TxResult = 'tesSUCCESS';)"));
    soci::statement st = (db->prepare << sql, soci::into(value));
    auto res = st.execute(true);

    if (res && *value)
        count = (*value);
    return count;
}

int 
LedgerAdjust::getTxFailCount(LockedSociSession db)
{
    boost::optional<int> value;
    int count = 0;
    std::string sql = boost::str(boost::format(
                    R"(select count(*) from Transactions WHERE TxResult != 'tesSUCCESS';)"));
    soci::statement st = (db->prepare << sql, soci::into(value));
    auto res = st.execute(true);

    if (res && *value)
        count = (*value);
    return count;
}

int 
LedgerAdjust::getContractCreateCount(LockedSociSession db)
{
    boost::optional<int> value;
    int count = 0;
    std::string sql = boost::str(boost::format(
                    R"(select count(distinct(Owner)) from TraceTransactions WHERE TransType = 'Contract';)"));
    soci::statement st = (db->prepare << sql, soci::into(value));
    auto res = st.execute(true);

    if (res && *value)
        count = (*value);
    return count;
}

int 
LedgerAdjust::getContractCallCount(LockedSociSession db)
{
    int count = 0;
    boost::optional<int> value;
    std::string sql = boost::str(boost::format(
                    R"(select count(*) from TraceTransactions WHERE TransType = 'Contract';)"));
    soci::statement st = (db->prepare << sql, soci::into(value));
    auto res = st.execute(true);

    if (res && *value)
        count = (*value);
    return count;
}

int 
LedgerAdjust::getAccountCount(LockedSociSession db)
{
    boost::optional<int> value;
    int count = 0;
    std::string sql = boost::str(boost::format(
                    R"(select count(distinct(Account)) from AccountTransactions;)"));
    soci::statement st = (db->prepare << sql, soci::into(value));
    auto res = st.execute(true);

    if (res && *value)
        count = (*value);
    if(count == 0)
        return 1;
    else 
        return count;
}

void
LedgerAdjust::createSle(Schema& app)
{
    if (!app.config().USE_TX_TABLES || !app.config().USE_TRACE_TABLE)
    {
        auto j = app.journal("LedgerAdjust");
        JLOG(j.error()) << "use_tx_tables or use_trace_tables is not configured ";
        return;
    }

    TxnDBCon& connection = app.getTxnDBCHECK();
    auto countSle = app.getPrometheusClient().getPromethSle();
    countSle->setFieldU32(sfTxSuccessCountField, getTxSucessCount(connection.checkoutDbRead()));
    countSle->setFieldU32(sfTxFailureCountField, getTxFailCount(connection.checkoutDbRead()));
    countSle->setFieldU32(sfContractCallCountField,getContractCallCount(connection.checkoutDbRead()));
    int contractCount = getContractCreateCount(connection.checkoutDbRead());
    countSle->setFieldU32(sfContractCreateCountField, contractCount);
    int accountCount = getAccountCount(connection.checkoutDbRead());
    if(accountCount > 1)
        accountCount = accountCount - contractCount;
    countSle->setFieldU32(sfAccountCountField, accountCount);
    
}

bool 
LedgerAdjust::isCompleteReadData(Schema& app)
{
    std::shared_ptr<SLE> countSle = app.getPrometheusClient().getPromethSle();
    if (!countSle->isFieldPresent(sfTxSuccessCountField))
        return false;
    if (!countSle->isFieldPresent(sfTxFailureCountField))
        return false;
    if (!countSle->isFieldPresent(sfContractCallCountField))
        return false;
    if (!countSle->isFieldPresent(sfContractCreateCountField))
        return false;
    if (!countSle->isFieldPresent(sfAccountCountField))
        return false;
    return true;
}

void
LedgerAdjust::updateContractCount(Schema& app,ApplyView& view, ContractState state)
{
    Keylet key = keylet::statis();
    auto countSle = view.peek(key);
    if (countSle)
    {
        int count = countSle->getFieldU32(sfContractCreateCountField);
        if (state == ContractState::CONTRACT_CREATE)
        {
            countSle->setFieldU32(sfContractCreateCountField, count + 1);
        }
        else if (state == ContractState::CONTRACT_CALL)
        {
            countSle->setFieldU32(sfContractCallCountField, countSle->getFieldU32(sfContractCallCountField) + 1);
        }
        else
        {
            if (count != 0)
                countSle->setFieldU32(sfContractCreateCountField, count - 1);
        }
        view.update(countSle);
        
    }
    else
    {
        if (!isCompleteReadData(app))
            return;
        countSle = app.getPrometheusClient().getPromethSle();
        if (state == ContractState::CONTRACT_CREATE)
        {
            countSle->setFieldU32(sfContractCreateCountField, countSle->getFieldU32(sfContractCreateCountField) + 1);
        }
        else if (state == ContractState::CONTRACT_CALL)
        {
            countSle->setFieldU32(sfContractCallCountField, countSle->getFieldU32(sfContractCallCountField) + 1);
        }
        view.insert(countSle);
    }
}

void
LedgerAdjust::updateTxCount(Schema& app, OpenView& view, int successCount, int failCount)
{
    Keylet key = keylet::statis();
    auto sle = view.read(key);
    std::shared_ptr<SLE> countSle;
    if (sle)
    {
        countSle = std::make_shared<SLE>(*sle);

        auto count_s = countSle->getFieldU32(sfTxSuccessCountField);
        auto count_f = countSle->getFieldU32(sfTxFailureCountField);
        count_s += successCount;
        count_f += failCount;
        countSle->setFieldU32(sfTxSuccessCountField, count_s);
        countSle->setFieldU32(sfTxFailureCountField, count_f);
        view.rawReplace(countSle);
    }
    else
    {
       
        if (!isCompleteReadData(app))
            return;
        countSle = app.getPrometheusClient().getPromethSle();
        countSle->setFieldU32(sfTxSuccessCountField, countSle->getFieldU32(sfTxSuccessCountField) + successCount);
        countSle->setFieldU32(sfTxFailureCountField, countSle->getFieldU32(sfTxFailureCountField) + failCount);
        view.rawInsert(countSle);
    }
}

void
LedgerAdjust::updateAccountCount(Schema& app, OpenView& view, int accountCount)
{
    Keylet key = keylet::statis();
    auto sle = view.read(key);
    std::shared_ptr<SLE> countSle;
    if (sle)
    {
        countSle = std::make_shared<SLE>(*sle);
        auto count = countSle->getFieldU32(sfAccountCountField);
        count += accountCount;
        countSle->setFieldU32(sfAccountCountField, count);
        view.rawReplace(countSle);
    }
    else
    {
        if (!isCompleteReadData(app))
            return;
        countSle = app.getPrometheusClient().getPromethSle();
        countSle->setFieldU32(sfAccountCountField, countSle->getFieldU32(sfAccountCountField) + accountCount);
        view.rawInsert(countSle);
    }
}



}

