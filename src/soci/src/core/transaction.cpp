//
// Copyright (C) 2004-2008 Maciej Sobczak
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#define SOCI_SOURCE
#include "soci/transaction.h"
#include "soci/error.h"

using namespace soci;

transaction::transaction(session& sql)
    : handled_(false), sql_(sql)
{
    sql_.begin();
}

transaction::~transaction()
{
    if (handled_ == false)
    {
        try
        {
            rollback();
        }
        catch (...)
        {}
    }
}

void transaction::commit()
{
    if (handled_)
    {
        throw soci_error("The transaction object cannot be handled twice.");
    }

    handled_ = true;
    sql_.commit();    

	// In Mycat, `autocommit` should be reset to true
	// after a transaction commit or rollback.
	if (sql_.autocommit_after_transaction()) {
		sql_.autocommit(true);
	}
}

void transaction::rollback()
{
    if (handled_)
    {
        throw soci_error("The transaction object cannot be handled twice.");
    }

    //throwing exception when rollback fistly, then destruct myself,
    //set handled_ to be true to avoid rollback secondly. 
    //same as modification in commit function
  
    handled_ = true;
    sql_.rollback();

	// In Mycat, `autocommit` should be reset to true
	// after a transaction commit or rollback.
	if (sql_.autocommit_after_transaction()) {
		sql_.autocommit(true);
	}
}