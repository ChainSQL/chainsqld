//------------------------------------------------------------------------------
/*
This file is part of rippled: https://github.com/ripple/rippled
Copyright (c) 2012, 2013 Ripple Labs Inc.

Permission to use, copy, modify, and/or distribute this software for any
purpose  with  or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <test/jtx/pay.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/TxFlags.h>

namespace ripple {
	namespace test {
		namespace jtx {

			Json::Value
				pay(Account const& account,
					Account const& to,
					AnyAmount amount)
			{
				amount.to(to);
				Json::Value jv;
				jv[jss::Account] = account.human();
				jv[jss::Amount] = amount.value.getJson(0);
				jv[jss::Destination] = to.human();
				jv[jss::TransactionType] = "Payment";
				jv[jss::Flags] = tfUniversal;
				return jv;
			}

			Json::Value
				paytableset(Account const& account)
			{
				Json::Value jv;
				jv[jss::Account] = account.human();
				jv[jss::TransactionType] = "TableListSet";
				jv["OpType"] = "1";
				jv["Raw"] = "7b276665696c64273a27616765272c2774797065273a27696e74277d";
				Json::Value tables;
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "31323334";
				tv["NameInDB"] = "4E3F8276DCF232D4CBBB7F51C849A99AD51701F6";
				t["Table"] = tv;
				tables.append(t);
				jv["Tables"] = tables;
				return jv;
			}

			Json::Value
				paysqlstatement(Account const& account)
			{
				Json::Value jv;
				jv[jss::Account] = account.human();
				jv[jss::TransactionType] = "SQLStatement";
				jv["Owner"] = account.human();
				jv["OpType"] = "6";
				jv["Raw"] = "7b27616765273a2032317d";
				Json::Value tables;
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "31323334";
				tv["NameInDB"] = "4E3F8276DCF232D4CBBB7F51C849A99AD51701F6";
				t["Table"] = tv;
				tables.append(t);
				jv["Tables"] = tables;
				return jv;
			}

		} // jtx
	} // test
} // ripple
