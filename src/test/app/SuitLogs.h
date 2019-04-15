
//------------------------------------------------------------------------------
/*
This file is part of chainsqld: https://github.com/chainsql/chainsqld
Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.

chainsqld is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

chainsqld is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
//==============================================================================
#ifndef SUITLOGS_H_INCLUDED
#define SUITLOGS_H_INCLUDED

#include <ripple/beast/utility/Journal.h>
#include <ripple/beast/unit_test.h>
#include <ripple/basics/Log.h>

#include <iostream>
using namespace std;

namespace ripple {
	class SuiteSink : public beast::Journal::Sink
	{
		std::string partition_;
		beast::unit_test::suite& suite_;

	public:
		SuiteSink(std::string const& partition,
			beast::severities::Severity threshold,
			beast::unit_test::suite& suite)
			: Sink(threshold, false)
			, partition_(partition + " ")
			, suite_(suite)
		{
		}

		// For unit testing, always generate logging text.
		bool active(beast::severities::Severity level) const override
		{
			return true;
		}

		void
			write(beast::severities::Severity level,
				std::string const& text) override
		{
			using namespace beast::severities;
			std::string s;
			switch (level)
			{
			case kTrace:    s = "TRC:"; break;
			case kDebug:    s = "DBG:"; break;
			case kInfo:     s = "INF:"; break;
			case kWarning:  s = "WRN:"; break;
			case kError:    s = "ERR:"; break;
			default:
			case kFatal:    s = "FTL:"; break;
			}

			// Only write the string if the level at least equals the threshold.
			if (level >= threshold())
				suite_.log << s << partition_ << text << std::endl;
		}
	};



	class SuiteLogs : public Logs
	{
		beast::unit_test::suite& suite_;

	public:
		explicit
			SuiteLogs(beast::unit_test::suite& suite)
			: Logs(beast::severities::kError)
			, suite_(suite)
		{
		}

		~SuiteLogs() override = default;

		std::unique_ptr<beast::Journal::Sink>
			makeSink(std::string const& partition,
				beast::severities::Severity threshold) override
		{
			return std::make_unique<SuiteSink>(partition, threshold, suite_);
		}
	};
}

#endif
