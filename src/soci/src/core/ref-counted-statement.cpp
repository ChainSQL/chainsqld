//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#define SOCI_SOURCE
#include "soci/ref-counted-statement.h"
#include "soci/session.h"

using namespace soci;
using namespace soci::details;

ref_counted_statement_base::ref_counted_statement_base(session& s)
    : refCount_(1)
    , session_(s)
{
}

void ref_counted_statement::final_action()
{
    try
    {
        st_.alloc();
        st_.prepare(session_.get_query(), st_one_time_query);
        st_.define_and_bind();

        const bool gotData = st_.execute(true);
        session_.set_got_data(gotData);
		session_.set_affected_row_count(st_.get_affected_rows());

		// fix an issue that we can't catch an exception on top-level,
		// beacause desctructor of one-temp-type driver to execute actual SQL-engine API.
		// however destructor can catch an exception but can't throw an exception that was catched by destructor.
		session_.set_last_error({0, "success"});
    }
	catch (soci::soci_error& e)
	{
		st_.clean_up();
		// fix an issue that we can't catch an exception on top-level,
		// beacause desctructor of one-temp-type driver to execute actual SQL-engine API.
		// however destructor can catch an exception but can't throw an exception that was catched by destructor.
		session_.set_last_error({ -1, e.what()});
        throw e;
	}
    catch (...)
    {
        st_.clean_up();
		std::string err_msg = "exec sql error.[" + session_.get_query() + "]";
		// fix an issue that we can't catch an exception on top-level,
		// beacause desctructor of one-temp-type driver to execute actual SQL-engine API.
		// however destructor can catch an exception but can't throw an exception that was catched by destructor.
		session_.set_last_error({ -1, err_msg });
        throw soci::soci_error(err_msg);
    }

    st_.clean_up();
}

std::ostringstream& ref_counted_statement_base::get_query_stream()
{
    return session_.get_query_stream();
}
