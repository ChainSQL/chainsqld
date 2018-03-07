#ifndef RIPPLE_TX_OPERATION_RULE_H_INCLUDED
#define RIPPLE_TX_OPERATION_RULE_H_INCLUDED

#include <ripple/app/tx/impl/Transactor.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/TxFlags.h>
#include <peersafe/app/tx/ChainSqlTx.h>

namespace ripple {

class OperationRule {
public:
	//for TableListSet
	static TER dealWithTableListSetRule(ApplyContext& ctx);
	//for SqlStatement
	static TER dealWithSqlStatementRule(ApplyContext& ctx);

	static std::string getOperationRule(ApplyView& view, const STTx& tx);
	static bool hasOperationRule(ApplyView& view, const STTx& tx);

	static TER adjustInsertCount(ApplyContext& ctx, DatabaseCon* pConn);
};

} // ripple
#endif