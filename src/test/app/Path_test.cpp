//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2015 Ripple Labs Inc.

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

#include <ripple/app/paths/AccountCurrencies.h>
#include <ripple/basics/contract.h>
#include <ripple/beast/unit_test.h>
#include <ripple/core/JobQueue.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/STParsedJSON.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/RPCHandler.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/impl/Tuning.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <test/jtx.h>
#include <thread>

namespace ripple {
namespace test {

//------------------------------------------------------------------------------

namespace detail {

void
stpath_append_one(STPath& st, jtx::Account const& account)
{
    st.push_back(STPathElement({account.id(), boost::none, boost::none}));
}

template <class T>
std::enable_if_t<std::is_constructible<jtx::Account, T>::value>
stpath_append_one(STPath& st, T const& t)
{
    stpath_append_one(st, jtx::Account{t});
}

void
stpath_append_one(STPath& st, jtx::IOU const& iou)
{
    st.push_back(STPathElement({iou.account.id(), iou.currency, boost::none}));
}

void
stpath_append_one(STPath& st, STPathElement const& pe)
{
    st.push_back(pe);
}

void
stpath_append_one(STPath& st, jtx::BookSpec const& book)
{
    st.push_back(STPathElement({boost::none, book.currency, book.account}));
}

template <class T, class... Args>
void
stpath_append(STPath& st, T const& t, Args const&... args)
{
    stpath_append_one(st, t);
    if constexpr (sizeof...(args) > 0)
        stpath_append(st, args...);
}

template <class... Args>
void
stpathset_append(STPathSet& st, STPath const& p, Args const&... args)
{
    st.push_back(p);
    if constexpr (sizeof...(args) > 0)
        stpathset_append(st, args...);
}

}  // namespace detail

template <class... Args>
STPath
stpath(Args const&... args)
{
    STPath st;
    detail::stpath_append(st, args...);
    return st;
}

template <class... Args>
bool
same(STPathSet const& st1, Args const&... args)
{
    STPathSet st2;
    detail::stpathset_append(st2, args...);
    if (st1.size() != st2.size())
        return false;

    for (auto const& p : st2)
    {
        if (std::find(st1.begin(), st1.end(), p) == st1.end())
            return false;
    }
    return true;
}

bool
equal(STAmount const& sa1, STAmount const& sa2)
{
    return sa1 == sa2 && sa1.issue().account == sa2.issue().account;
}

Json::Value
rpf(jtx::Account const& src, jtx::Account const& dst, std::uint32_t num_src)
{
    Json::Value jv = Json::objectValue;
    jv[jss::command] = "ripple_path_find";
    jv[jss::source_account] = toBase58(src);

    if (num_src > 0)
    {
        auto& sc = (jv[jss::source_currencies] = Json::arrayValue);
        Json::Value j = Json::objectValue;
        while (num_src--)
        {
            j[jss::currency] = std::to_string(num_src + 100);
            sc.append(j);
        }
    }

    auto const d = toBase58(dst);
    jv[jss::destination_account] = d;

    Json::Value& j = (jv[jss::destination_amount] = Json::objectValue);
    j[jss::currency] = "USD";
    j[jss::value] = "0.01";
    j[jss::issuer] = d;

    return jv;
}

// Issue path element
auto
IPE(Issue const& iss)
{
    return STPathElement(
        STPathElement::typeCurrency | STPathElement::typeIssuer,
        zxcAccount(),
        iss.currency,
        iss.account);
};

//------------------------------------------------------------------------------

class Path_test : public beast::unit_test::suite
{
public:
    class gate
    {
    private:
        std::condition_variable cv_;
        std::mutex mutex_;
        bool signaled_ = false;

    public:
        // Thread safe, blocks until signaled or period expires.
        // Returns `true` if signaled.
        template <class Rep, class Period>
        bool
        wait_for(std::chrono::duration<Rep, Period> const& rel_time)
        {
            std::unique_lock<std::mutex> lk(mutex_);
            auto b = cv_.wait_for(lk, rel_time, [=] { return signaled_; });
            signaled_ = false;
            return b;
        }

        void
        signal()
        {
            std::lock_guard lk(mutex_);
            signaled_ = true;
            cv_.notify_all();
        }
    };

    auto
    find_paths_request(
        jtx::Env& env,
        jtx::Account const& src,
        jtx::Account const& dst,
        STAmount const& saDstAmount,
        boost::optional<STAmount> const& saSendMax = boost::none,
        boost::optional<Currency> const& saSrcCurrency = boost::none)
    {
        using namespace jtx;

        auto& app = env.app();
        Resource::Charge loadType = Resource::feeReferenceRPC;
        Resource::Consumer c;

        RPC::JsonContext context{
            {env.journal,
             app,
             loadType,
             app.getOPs(),
             app.getLedgerMaster(),
             c,
             Role::USER,
             {},
             {},
             RPC::APIVersionIfUnspecified},
            {},
            {}};

        Json::Value params = Json::objectValue;
        params[jss::command] = "ripple_path_find";
        params[jss::source_account] = toBase58(src);
        params[jss::destination_account] = toBase58(dst);
        params[jss::destination_amount] =
            saDstAmount.getJson(JsonOptions::none);
        if (saSendMax)
            params[jss::send_max] = saSendMax->getJson(JsonOptions::none);
        if (saSrcCurrency)
        {
            auto& sc = params[jss::source_currencies] = Json::arrayValue;
            Json::Value j = Json::objectValue;
            j[jss::currency] = to_string(saSrcCurrency.value());
            sc.append(j);
        }

        Json::Value result;
        gate g;
        app.getJobQueue().postCoro(
            jtCLIENT, "RPC-Client", [&](auto const& coro) {
                context.params = std::move(params);
                context.coro = coro;
                RPC::doCommand(context, result);
                g.signal();
            });

        using namespace std::chrono_literals;
        BEAST_EXPECT(g.wait_for(5s));
        BEAST_EXPECT(!result.isMember(jss::error));
        return result;
    }

    std::tuple<STPathSet, STAmount, STAmount>
    find_paths(
        jtx::Env& env,
        jtx::Account const& src,
        jtx::Account const& dst,
        STAmount const& saDstAmount,
        boost::optional<STAmount> const& saSendMax = boost::none,
        boost::optional<Currency> const& saSrcCurrency = boost::none)
    {
        Json::Value result = find_paths_request(
            env, src, dst, saDstAmount, saSendMax, saSrcCurrency);
        BEAST_EXPECT(!result.isMember(jss::error));

        STAmount da;
        if (result.isMember(jss::destination_amount))
            da = amountFromJson(sfGeneric, result[jss::destination_amount]);

        STAmount sa;
        STPathSet paths;
        if (result.isMember(jss::alternatives))
        {
            auto const& alts = result[jss::alternatives];
            if (alts.size() > 0)
            {
                auto const& path = alts[0u];

                if (path.isMember(jss::source_amount))
                    sa = amountFromJson(sfGeneric, path[jss::source_amount]);

                if (path.isMember(jss::destination_amount))
                    da = amountFromJson(
                        sfGeneric, path[jss::destination_amount]);

                if (path.isMember(jss::paths_computed))
                {
                    Json::Value p;
                    p["Paths"] = path[jss::paths_computed];
                    STParsedJSONObject po("generic", p);
                    paths = po.object->getFieldPathSet(sfPaths);
                }
            }
        }

        return std::make_tuple(std::move(paths), std::move(sa), std::move(da));
    }

    void
    source_currencies_limit()
    {
        testcase("source currency limits");
        using namespace std::chrono_literals;
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        env.fund(ZXC(10000), "alice", "bob", gw);
        env.trust(gw["USD"](100), "alice", "bob");
        env.close();

        auto& app = env.app();
        Resource::Charge loadType = Resource::feeReferenceRPC;
        Resource::Consumer c;

        RPC::JsonContext context{
            {env.journal,
             app,
             loadType,
             app.getOPs(),
             app.getLedgerMaster(),
             c,
             Role::USER,
             {},
             {},
             RPC::APIVersionIfUnspecified},
            {},
            {}};
        Json::Value result;
        gate g;
        // Test RPC::Tuning::max_src_cur source currencies.
        app.getJobQueue().postCoro(
            jtCLIENT, "RPC-Client", [&](auto const& coro) {
                context.params = rpf(
                    Account("alice"), Account("bob"), RPC::Tuning::max_src_cur);
                context.coro = coro;
                RPC::doCommand(context, result);
                g.signal();
            });
        BEAST_EXPECT(g.wait_for(5s));
        BEAST_EXPECT(!result.isMember(jss::error));

        // Test more than RPC::Tuning::max_src_cur source currencies.
        app.getJobQueue().postCoro(
            jtCLIENT, "RPC-Client", [&](auto const& coro) {
                context.params =
                    rpf(Account("alice"),
                        Account("bob"),
                        RPC::Tuning::max_src_cur + 1);
                context.coro = coro;
                RPC::doCommand(context, result);
                g.signal();
            });
        BEAST_EXPECT(g.wait_for(5s));
        BEAST_EXPECT(result.isMember(jss::error));

        // Test RPC::Tuning::max_auto_src_cur source currencies.
        for (auto i = 0; i < (RPC::Tuning::max_auto_src_cur - 1); ++i)
            env.trust(Account("alice")[std::to_string(i + 100)](100), "bob");
        app.getJobQueue().postCoro(
            jtCLIENT, "RPC-Client", [&](auto const& coro) {
                context.params = rpf(Account("alice"), Account("bob"), 0);
                context.coro = coro;
                RPC::doCommand(context, result);
                g.signal();
            });
        BEAST_EXPECT(g.wait_for(5s));
        BEAST_EXPECT(!result.isMember(jss::error));

        // Test more than RPC::Tuning::max_auto_src_cur source currencies.
        env.trust(Account("alice")["AUD"](100), "bob");
        app.getJobQueue().postCoro(
            jtCLIENT, "RPC-Client", [&](auto const& coro) {
                context.params = rpf(Account("alice"), Account("bob"), 0);
                context.coro = coro;
                RPC::doCommand(context, result);
                g.signal();
            });
        BEAST_EXPECT(g.wait_for(5s));
        BEAST_EXPECT(result.isMember(jss::error));
    }

    void
    no_direct_path_no_intermediary_no_alternatives()
    {
        testcase("no direct path no intermediary no alternatives");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");

        auto const result =
            find_paths(env, "alice", "bob", Account("bob")["USD"](5));
        BEAST_EXPECT(std::get<0>(result).empty());
    }

    void
    direct_path_no_intermediary()
    {
        testcase("direct path no intermediary");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");
        env.trust(Account("alice")["USD"](700), "bob");

        STPathSet st;
        STAmount sa;
        std::tie(st, sa, std::ignore) =
            find_paths(env, "alice", "bob", Account("bob")["USD"](5));
        BEAST_EXPECT(st.empty());
        BEAST_EXPECT(equal(sa, Account("alice")["USD"](5)));
    }

    void
    payment_auto_path_find()
    {
        testcase("payment auto path find");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        env.fund(ZXC(10000), "alice", "bob", gw);
        env.trust(USD(600), "alice");
        env.trust(USD(700), "bob");
        env(pay(gw, "alice", USD(70)));
        env(pay("alice", "bob", USD(24)));
        env.require(balance("alice", USD(46)));
        env.require(balance(gw, Account("alice")["USD"](-46)));
        env.require(balance("bob", USD(24)));
        env.require(balance(gw, Account("bob")["USD"](-24)));
    }

    void
    path_find()
    {
        testcase("path find");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        env.fund(ZXC(10000), "alice", "bob", gw);
        env.trust(USD(600), "alice");
        env.trust(USD(700), "bob");
        env(pay(gw, "alice", USD(70)));
        env(pay(gw, "bob", USD(50)));

        STPathSet st;
        STAmount sa;
        std::tie(st, sa, std::ignore) =
            find_paths(env, "alice", "bob", Account("bob")["USD"](5));
        BEAST_EXPECT(same(st, stpath("gateway")));
        BEAST_EXPECT(equal(sa, Account("alice")["USD"](5)));
    }

    void
    zxc_to_zxc()
    {
        using namespace jtx;
        testcase("ZXC to ZXC");
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");

        auto const result = find_paths(env, "alice", "bob", ZXC(5));
        BEAST_EXPECT(std::get<0>(result).empty());
    }

    void
    path_find_consume_all()
    {
        testcase("path find consume all");
        using namespace jtx;

        {
            Env env(*this);
            env.fund(ZXC(10000), "alice", "bob", "carol", "dan", "edward");
            env.trust(Account("alice")["USD"](10), "bob");
            env.trust(Account("bob")["USD"](10), "carol");
            env.trust(Account("carol")["USD"](10), "edward");
            env.trust(Account("alice")["USD"](100), "dan");
            env.trust(Account("dan")["USD"](100), "edward");

            STPathSet st;
            STAmount sa;
            STAmount da;
            std::tie(st, sa, da) = find_paths(
                env, "alice", "edward", Account("edward")["USD"](-1));
            BEAST_EXPECT(same(st, stpath("dan"), stpath("bob", "carol")));
            BEAST_EXPECT(equal(sa, Account("alice")["USD"](110)));
            BEAST_EXPECT(equal(da, Account("edward")["USD"](110)));
        }

        {
            Env env(*this);
            auto const gw = Account("gateway");
            auto const USD = gw["USD"];
            env.fund(ZXC(10000), "alice", "bob", "carol", gw);
            env.trust(USD(100), "bob", "carol");
            env(pay(gw, "carol", USD(100)));
            env(offer("carol", ZXC(100), USD(100)));

            STPathSet st;
            STAmount sa;
            STAmount da;
            std::tie(st, sa, da) = find_paths(
                env,
                "alice",
                "bob",
                Account("bob")["AUD"](-1),
                boost::optional<STAmount>(ZXC(100000000)));
            BEAST_EXPECT(st.empty());
            std::tie(st, sa, da) = find_paths(
                env,
                "alice",
                "bob",
                Account("bob")["USD"](-1),
                boost::optional<STAmount>(ZXC(100000000)));
            BEAST_EXPECT(sa == ZXC(100));
            BEAST_EXPECT(equal(da, Account("bob")["USD"](100)));
        }
    }

    void
    alternative_path_consume_both()
    {
        testcase("alternative path consume both");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        auto const gw2 = Account("gateway2");
        auto const gw2_USD = gw2["USD"];
        env.fund(ZXC(10000), "alice", "bob", gw, gw2);
        env.trust(USD(600), "alice");
        env.trust(gw2_USD(800), "alice");
        env.trust(USD(700), "bob");
        env.trust(gw2_USD(900), "bob");
        env(pay(gw, "alice", USD(70)));
        env(pay(gw2, "alice", gw2_USD(70)));
        env(pay("alice", "bob", Account("bob")["USD"](140)),
            paths(Account("alice")["USD"]));
        env.require(balance("alice", USD(0)));
        env.require(balance("alice", gw2_USD(0)));
        env.require(balance("bob", USD(70)));
        env.require(balance("bob", gw2_USD(70)));
        env.require(balance(gw, Account("alice")["USD"](0)));
        env.require(balance(gw, Account("bob")["USD"](-70)));
        env.require(balance(gw2, Account("alice")["USD"](0)));
        env.require(balance(gw2, Account("bob")["USD"](-70)));
    }

    void
    alternative_paths_consume_best_transfer()
    {
        testcase("alternative paths consume best transfer");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        auto const gw2 = Account("gateway2");
        auto const gw2_USD = gw2["USD"];
        env.fund(ZXC(10000), "alice", "bob", gw, gw2);
        env(rate(gw2, 1.1));
        env.trust(USD(600), "alice");
        env.trust(gw2_USD(800), "alice");
        env.trust(USD(700), "bob");
        env.trust(gw2_USD(900), "bob");
        env(pay(gw, "alice", USD(70)));
        env(pay(gw2, "alice", gw2_USD(70)));
        env(pay("alice", "bob", USD(70)));
        env.require(balance("alice", USD(0)));
        env.require(balance("alice", gw2_USD(70)));
        env.require(balance("bob", USD(70)));
        env.require(balance("bob", gw2_USD(0)));
        env.require(balance(gw, Account("alice")["USD"](0)));
        env.require(balance(gw, Account("bob")["USD"](-70)));
        env.require(balance(gw2, Account("alice")["USD"](-70)));
        env.require(balance(gw2, Account("bob")["USD"](0)));
    }

    void
    alternative_paths_consume_best_transfer_first()
    {
        testcase("alternative paths - consume best transfer first");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        auto const gw2 = Account("gateway2");
        auto const gw2_USD = gw2["USD"];
        env.fund(ZXC(10000), "alice", "bob", gw, gw2);
        env(rate(gw2, 1.1));
        env.trust(USD(600), "alice");
        env.trust(gw2_USD(800), "alice");
        env.trust(USD(700), "bob");
        env.trust(gw2_USD(900), "bob");
        env(pay(gw, "alice", USD(70)));
        env(pay(gw2, "alice", gw2_USD(70)));
        env(pay("alice", "bob", Account("bob")["USD"](77)),
            sendmax(Account("alice")["USD"](100)),
            paths(Account("alice")["USD"]));
        env.require(balance("alice", USD(0)));
        env.require(balance("alice", gw2_USD(62.3)));
        env.require(balance("bob", USD(70)));
        env.require(balance("bob", gw2_USD(7)));
        env.require(balance(gw, Account("alice")["USD"](0)));
        env.require(balance(gw, Account("bob")["USD"](-70)));
        env.require(balance(gw2, Account("alice")["USD"](-62.3)));
        env.require(balance(gw2, Account("bob")["USD"](-7)));
    }

    void
    alternative_paths_limit_returned_paths_to_best_quality()
    {
        testcase("alternative paths - limit returned paths to best quality");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];
        auto const gw2 = Account("gateway2");
        auto const gw2_USD = gw2["USD"];
        env.fund(ZXC(10000), "alice", "bob", "carol", "dan", gw, gw2);
        env(rate("carol", 1.1));
        env.trust(Account("carol")["USD"](800), "alice", "bob");
        env.trust(Account("dan")["USD"](800), "alice", "bob");
        env.trust(USD(800), "alice", "bob");
        env.trust(gw2_USD(800), "alice", "bob");
        env.trust(Account("alice")["USD"](800), "dan");
        env.trust(Account("bob")["USD"](800), "dan");
        env(pay(gw2, "alice", gw2_USD(100)));
        env(pay("carol", "alice", Account("carol")["USD"](100)));
        env(pay(gw, "alice", USD(100)));

        STPathSet st;
        STAmount sa;
        std::tie(st, sa, std::ignore) =
            find_paths(env, "alice", "bob", Account("bob")["USD"](5));
        BEAST_EXPECT(same(
            st,
            stpath("gateway"),
            stpath("gateway2"),
            stpath("dan"),
            stpath("carol")));
        BEAST_EXPECT(equal(sa, Account("alice")["USD"](5)));
    }

    void
    issues_path_negative_issue()
    {
        testcase("path negative: Issue #5");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob", "carol", "dan");
        env.trust(Account("bob")["USD"](100), "alice", "carol", "dan");
        env.trust(Account("alice")["USD"](100), "dan");
        env.trust(Account("carol")["USD"](100), "dan");
        env(pay("bob", "carol", Account("bob")["USD"](75)));
        env.require(balance("bob", Account("carol")["USD"](-75)));
        env.require(balance("carol", Account("bob")["USD"](75)));

        auto result =
            find_paths(env, "alice", "bob", Account("bob")["USD"](25));
        BEAST_EXPECT(std::get<0>(result).empty());

        env(pay("alice", "bob", Account("alice")["USD"](25)), ter(tecPATH_DRY));

        result = find_paths(env, "alice", "bob", Account("alice")["USD"](25));
        BEAST_EXPECT(std::get<0>(result).empty());

        env.require(balance("alice", Account("bob")["USD"](0)));
        env.require(balance("alice", Account("dan")["USD"](0)));
        env.require(balance("bob", Account("alice")["USD"](0)));
        env.require(balance("bob", Account("carol")["USD"](-75)));
        env.require(balance("bob", Account("dan")["USD"](0)));
        env.require(balance("carol", Account("bob")["USD"](75)));
        env.require(balance("carol", Account("dan")["USD"](0)));
        env.require(balance("dan", Account("alice")["USD"](0)));
        env.require(balance("dan", Account("bob")["USD"](0)));
        env.require(balance("dan", Account("carol")["USD"](0)));
    }

    // alice -- limit 40 --> bob
    // alice --> carol --> dan --> bob
    // Balance of 100 USD Bob - Balance of 37 USD -> Rod
    void
    issues_path_negative_ripple_client_issue_23_smaller()
    {
        testcase("path negative: ripple-client issue #23: smaller");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob", "carol", "dan");
        env.trust(Account("alice")["USD"](40), "bob");
        env.trust(Account("dan")["USD"](20), "bob");
        env.trust(Account("alice")["USD"](20), "carol");
        env.trust(Account("carol")["USD"](20), "dan");
        env(pay("alice", "bob", Account("bob")["USD"](55)),
            paths(Account("alice")["USD"]));
        env.require(balance("bob", Account("alice")["USD"](40)));
        env.require(balance("bob", Account("dan")["USD"](15)));
    }

    // alice -120 USD-> edward -25 USD-> bob
    // alice -25 USD-> carol -75 USD -> dan -100 USD-> bob
    void
    issues_path_negative_ripple_client_issue_23_larger()
    {
        testcase("path negative: ripple-client issue #23: larger");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob", "carol", "dan", "edward");
        env.trust(Account("alice")["USD"](120), "edward");
        env.trust(Account("edward")["USD"](25), "bob");
        env.trust(Account("dan")["USD"](100), "bob");
        env.trust(Account("alice")["USD"](25), "carol");
        env.trust(Account("carol")["USD"](75), "dan");
        env(pay("alice", "bob", Account("bob")["USD"](50)),
            paths(Account("alice")["USD"]));
        env.require(balance("alice", Account("edward")["USD"](-25)));
        env.require(balance("alice", Account("carol")["USD"](-25)));
        env.require(balance("bob", Account("edward")["USD"](25)));
        env.require(balance("bob", Account("dan")["USD"](25)));
        env.require(balance("carol", Account("alice")["USD"](25)));
        env.require(balance("carol", Account("dan")["USD"](-25)));
        env.require(balance("dan", Account("carol")["USD"](25)));
        env.require(balance("dan", Account("bob")["USD"](-25)));
    }

    // carol holds gateway AUD, sells gateway AUD for ZXC
    // bob will hold gateway AUD
    // alice pays bob gateway AUD using ZXC
    void
    via_offers_via_gateway()
    {
        testcase("via gateway");
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const AUD = gw["AUD"];
        env.fund(ZXC(10000), "alice", "bob", "carol", gw);
        env(rate(gw, 1.1));
        env.trust(AUD(100), "bob", "carol");
        env(pay(gw, "carol", AUD(50)));
        env(offer("carol", ZXC(50), AUD(50)));
        env(pay("alice", "bob", AUD(10)), sendmax(ZXC(100)), paths(ZXC));
        env.require(balance("bob", AUD(10)));
        env.require(balance("carol", AUD(39)));

        auto const result =
            find_paths(env, "alice", "bob", Account("bob")["USD"](25));
        BEAST_EXPECT(std::get<0>(result).empty());
    }

    void
    indirect_paths_path_find()
    {
        testcase("path find");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob", "carol");
        env.trust(Account("alice")["USD"](1000), "bob");
        env.trust(Account("bob")["USD"](1000), "carol");

        STPathSet st;
        STAmount sa;
        std::tie(st, sa, std::ignore) =
            find_paths(env, "alice", "carol", Account("carol")["USD"](5));
        BEAST_EXPECT(same(st, stpath("bob")));
        BEAST_EXPECT(equal(sa, Account("alice")["USD"](5)));
    }

    void
    quality_paths_quality_set_and_test()
    {
        testcase("quality set and test");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");
        env(trust("bob", Account("alice")["USD"](1000)),
            json("{\"" + sfQualityIn.fieldName + "\": 2000}"),
            json("{\"" + sfQualityOut.fieldName + "\": 1400000000}"));

        Json::Value jv;
        Json::Reader().parse(
            R"({
                "Balance" : {
                    "currency" : "USD",
                    "issuer" : "rrrrrrrrrrrrrrrrrrrrBZbvji",
                    "value" : "0"
                },
                "Flags" : 131072,
                "HighLimit" : {
                    "currency" : "USD",
                    "issuer" : "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
                    "value" : "1000"
                },
                "HighNode" : "0000000000000000",
                "HighQualityIn" : 2000,
                "HighQualityOut" : 1400000000,
                "LedgerEntryType" : "RippleState",
                "LowLimit" : {
                    "currency" : "USD",
                    "issuer" : "rG1QQv2nh2gr7RCZ1P8YYcBUKCCN633jCn",
                    "value" : "0"
                },
                "LowNode" : "0000000000000000"
            })",
            jv);

        auto const jv_l =
            env.le(keylet::line(
                       Account("bob").id(), Account("alice")["USD"].issue()))
                ->getJson(JsonOptions::none);
        for (auto it = jv.begin(); it != jv.end(); ++it)
            BEAST_EXPECT(*it == jv_l[it.memberName()]);
    }

    void
    trust_auto_clear_trust_normal_clear()
    {
        testcase("trust normal clear");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");
        env.trust(Account("bob")["USD"](1000), "alice");
        env.trust(Account("alice")["USD"](1000), "bob");

        Json::Value jv;
        Json::Reader().parse(
            R"({
                "Balance" : {
                    "currency" : "USD",
                    "issuer" : "rrrrrrrrrrrrrrrrrrrrBZbvji",
                    "value" : "0"
                },
                "Flags" : 196608,
                "HighLimit" : {
                    "currency" : "USD",
                    "issuer" : "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
                    "value" : "1000"
                },
                "HighNode" : "0000000000000000",
                "LedgerEntryType" : "RippleState",
                "LowLimit" : {
                    "currency" : "USD",
                    "issuer" : "rG1QQv2nh2gr7RCZ1P8YYcBUKCCN633jCn",
                    "value" : "1000"
                },
                "LowNode" : "0000000000000000"
            })",
            jv);

        auto const jv_l =
            env.le(keylet::line(
                       Account("bob").id(), Account("alice")["USD"].issue()))
                ->getJson(JsonOptions::none);
        for (auto it = jv.begin(); it != jv.end(); ++it)
            BEAST_EXPECT(*it == jv_l[it.memberName()]);

        env.trust(Account("bob")["USD"](0), "alice");
        env.trust(Account("alice")["USD"](0), "bob");
        BEAST_EXPECT(
            env.le(keylet::line(
                Account("bob").id(), Account("alice")["USD"].issue())) ==
            nullptr);
    }

    void
    trust_auto_clear_trust_auto_clear()
    {
        testcase("trust auto clear");
        using namespace jtx;
        Env env(*this);
        env.fund(ZXC(10000), "alice", "bob");
        env.trust(Account("bob")["USD"](1000), "alice");
        env(pay("bob", "alice", Account("bob")["USD"](50)));
        env.trust(Account("bob")["USD"](0), "alice");

        Json::Value jv;
        Json::Reader().parse(
            R"({
                "Balance" :
                {
                    "currency" : "USD",
                    "issuer" : "rrrrrrrrrrrrrrrrrrrrBZbvji",
                    "value" : "50"
                },
                "Flags" : 65536,
                "HighLimit" :
                {
                    "currency" : "USD",
                    "issuer" : "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
                    "value" : "0"
                },
                "HighNode" : "0000000000000000",
                "LedgerEntryType" : "RippleState",
                "LowLimit" :
                {
                    "currency" : "USD",
                    "issuer" : "rG1QQv2nh2gr7RCZ1P8YYcBUKCCN633jCn",
                    "value" : "0"
                },
                "LowNode" : "0000000000000000"
            })",
            jv);

        auto const jv_l =
            env.le(keylet::line(
                       Account("alice").id(), Account("bob")["USD"].issue()))
                ->getJson(JsonOptions::none);
        for (auto it = jv.begin(); it != jv.end(); ++it)
            BEAST_EXPECT(*it == jv_l[it.memberName()]);

        env(pay("alice", "bob", Account("alice")["USD"](50)));
        BEAST_EXPECT(
            env.le(keylet::line(
                Account("alice").id(), Account("bob")["USD"].issue())) ==
            nullptr);
    }

    void
    path_find_01()
    {
        testcase("Path Find: ZXC -> ZXC and ZXC -> IOU");
        using namespace jtx;
        Env env(*this);
        Account A1{"A1"};
        Account A2{"A2"};
        Account A3{"A3"};
        Account G1{"G1"};
        Account G2{"G2"};
        Account G3{"G3"};
        Account M1{"M1"};

        env.fund(ZXC(100000), A1);
        env.fund(ZXC(10000), A2);
        env.fund(ZXC(1000), A3, G1, G2, G3, M1);
        env.close();

        env.trust(G1["XYZ"](5000), A1);
        env.trust(G3["ABC"](5000), A1);
        env.trust(G2["XYZ"](5000), A2);
        env.trust(G3["ABC"](5000), A2);
        env.trust(A2["ABC"](1000), A3);
        env.trust(G1["XYZ"](100000), M1);
        env.trust(G2["XYZ"](100000), M1);
        env.trust(G3["ABC"](100000), M1);
        env.close();

        env(pay(G1, A1, G1["XYZ"](3500)));
        env(pay(G3, A1, G3["ABC"](1200)));
        env(pay(G2, M1, G2["XYZ"](25000)));
        env(pay(G3, M1, G3["ABC"](25000)));
        env.close();

        env(offer(M1, G1["XYZ"](1000), G2["XYZ"](1000)));
        env(offer(M1, ZXC(10000), G3["ABC"](1000)));

        STPathSet st;
        STAmount sa, da;

        {
            auto const& send_amt = ZXC(10);
            std::tie(st, sa, da) =
                find_paths(env, A1, A2, send_amt, boost::none, zxcCurrency());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(st.empty());
        }

        {
            // no path should exist for this since dest account
            // does not exist.
            auto const& send_amt = ZXC(200);
            std::tie(st, sa, da) = find_paths(
                env, A1, Account{"A0"}, send_amt, boost::none, zxcCurrency());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(st.empty());
        }

        {
            auto const& send_amt = G3["ABC"](10);
            std::tie(st, sa, da) =
                find_paths(env, A2, G3, send_amt, boost::none, zxcCurrency());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, ZXC(100)));
            BEAST_EXPECT(same(st, stpath(IPE(G3["ABC"]))));
        }

        {
            auto const& send_amt = A2["ABC"](1);
            std::tie(st, sa, da) =
                find_paths(env, A1, A2, send_amt, boost::none, zxcCurrency());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, ZXC(10)));
            BEAST_EXPECT(same(st, stpath(IPE(G3["ABC"]), G3)));
        }

        {
            auto const& send_amt = A3["ABC"](1);
            std::tie(st, sa, da) =
                find_paths(env, A1, A3, send_amt, boost::none, zxcCurrency());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, ZXC(10)));
            BEAST_EXPECT(same(st, stpath(IPE(G3["ABC"]), G3, A2)));
        }
    }

    void
    path_find_02()
    {
        testcase("Path Find: non-ZXC -> ZXC");
        using namespace jtx;
        Env env(*this);
        Account A1{"A1"};
        Account A2{"A2"};
        Account G3{"G3"};
        Account M1{"M1"};

        env.fund(ZXC(1000), A1, A2, G3);
        env.fund(ZXC(11000), M1);
        env.close();

        env.trust(G3["ABC"](1000), A1, A2);
        env.trust(G3["ABC"](100000), M1);
        env.close();

        env(pay(G3, A1, G3["ABC"](1000)));
        env(pay(G3, A2, G3["ABC"](1000)));
        env(pay(G3, M1, G3["ABC"](1200)));
        env.close();

        env(offer(M1, G3["ABC"](1000), ZXC(10000)));

        STPathSet st;
        STAmount sa, da;

        auto const& send_amt = ZXC(10);
        std::tie(st, sa, da) =
            find_paths(env, A1, A2, send_amt, boost::none, A2["ABC"].currency);
        BEAST_EXPECT(equal(da, send_amt));
        BEAST_EXPECT(equal(sa, A1["ABC"](1)));
        BEAST_EXPECT(same(st, stpath(G3, IPE(zxcIssue()))));
    }

    void
    path_find_04()
    {
        testcase("Path Find: Bitstamp and SnapSwap, liquidity with no offers");
        using namespace jtx;
        Env env(*this);
        Account A1{"A1"};
        Account A2{"A2"};
        Account G1BS{"G1BS"};
        Account G2SW{"G2SW"};
        Account M1{"M1"};

        env.fund(ZXC(1000), G1BS, G2SW, A1, A2);
        env.fund(ZXC(11000), M1);
        env.close();

        env.trust(G1BS["HKD"](2000), A1);
        env.trust(G2SW["HKD"](2000), A2);
        env.trust(G1BS["HKD"](100000), M1);
        env.trust(G2SW["HKD"](100000), M1);
        env.close();

        env(pay(G1BS, A1, G1BS["HKD"](1000)));
        env(pay(G2SW, A2, G2SW["HKD"](1000)));
        // SnapSwap wants to be able to set trust line quality settings so they
        // can charge a fee when transactions ripple across. Liquidity
        // provider, via trusting/holding both accounts
        env(pay(G1BS, M1, G1BS["HKD"](1200)));
        env(pay(G2SW, M1, G2SW["HKD"](5000)));
        env.close();

        STPathSet st;
        STAmount sa, da;

        {
            auto const& send_amt = A2["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, A2, send_amt, boost::none, A2["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
            BEAST_EXPECT(same(st, stpath(G1BS, M1, G2SW)));
        }

        {
            auto const& send_amt = A1["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A2, A1, send_amt, boost::none, A1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A2["HKD"](10)));
            BEAST_EXPECT(same(st, stpath(G2SW, M1, G1BS)));
        }

        {
            auto const& send_amt = A2["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, G1BS, A2, send_amt, boost::none, A1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, G1BS["HKD"](10)));
            BEAST_EXPECT(same(st, stpath(M1, G2SW)));
        }

        {
            auto const& send_amt = M1["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, M1, G1BS, send_amt, boost::none, A1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, M1["HKD"](10)));
            BEAST_EXPECT(st.empty());
        }

        {
            auto const& send_amt = A1["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, G2SW, A1, send_amt, boost::none, A1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, G2SW["HKD"](10)));
            BEAST_EXPECT(same(st, stpath(M1, G1BS)));
        }
    }

    void
    path_find_05()
    {
        testcase("Path Find: non-ZXC -> non-ZXC, same currency");
        using namespace jtx;
        Env env(*this);
        Account A1{"A1"};
        Account A2{"A2"};
        Account A3{"A3"};
        Account A4{"A4"};
        Account G1{"G1"};
        Account G2{"G2"};
        Account G3{"G3"};
        Account G4{"G4"};
        Account M1{"M1"};
        Account M2{"M2"};

        env.fund(ZXC(1000), A1, A2, A3, G1, G2, G3, G4);
        env.fund(ZXC(10000), A4);
        env.fund(ZXC(11000), M1, M2);
        env.close();

        env.trust(G1["HKD"](2000), A1);
        env.trust(G2["HKD"](2000), A2);
        env.trust(G1["HKD"](2000), A3);
        env.trust(G1["HKD"](100000), M1);
        env.trust(G2["HKD"](100000), M1);
        env.trust(G1["HKD"](100000), M2);
        env.trust(G2["HKD"](100000), M2);
        env.close();

        env(pay(G1, A1, G1["HKD"](1000)));
        env(pay(G2, A2, G2["HKD"](1000)));
        env(pay(G1, A3, G1["HKD"](1000)));
        env(pay(G1, M1, G1["HKD"](1200)));
        env(pay(G2, M1, G2["HKD"](5000)));
        env(pay(G1, M2, G1["HKD"](1200)));
        env(pay(G2, M2, G2["HKD"](5000)));
        env.close();

        env(offer(M1, G1["HKD"](1000), G2["HKD"](1000)));
        env(offer(M2, ZXC(10000), G2["HKD"](1000)));
        env(offer(M2, G1["HKD"](1000), ZXC(10000)));

        STPathSet st;
        STAmount sa, da;

        {
            // A) Borrow or repay --
            //  Source -> Destination (repay source issuer)
            auto const& send_amt = G1["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, G1, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(st.empty());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
        }

        {
            // A2) Borrow or repay --
            //  Source -> Destination (repay destination issuer)
            auto const& send_amt = A1["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, G1, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(st.empty());
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
        }

        {
            // B) Common gateway --
            //  Source -> AC -> Destination
            auto const& send_amt = A3["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, A3, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
            BEAST_EXPECT(same(st, stpath(G1)));
        }

        {
            // C) Gateway to gateway --
            //  Source -> OB -> Destination
            auto const& send_amt = G2["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, G1, G2, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, G1["HKD"](10)));
            BEAST_EXPECT(same(
                st,
                stpath(IPE(G2["HKD"])),
                stpath(M1),
                stpath(M2),
                stpath(IPE(zxcIssue()), IPE(G2["HKD"]))));
        }

        {
            // D) User to unlinked gateway via order book --
            //  Source -> AC -> OB -> Destination
            auto const& send_amt = G2["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, G2, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
            BEAST_EXPECT(same(
                st,
                stpath(G1, M1),
                stpath(G1, M2),
                stpath(G1, IPE(G2["HKD"])),
                stpath(G1, IPE(zxcIssue()), IPE(G2["HKD"]))));
        }

        {
            // I4) ZXC bridge" --
            //  Source -> AC -> OB to ZXC -> OB from ZXC -> AC -> Destination
            auto const& send_amt = A2["HKD"](10);
            std::tie(st, sa, da) = find_paths(
                env, A1, A2, send_amt, boost::none, G1["HKD"].currency);
            BEAST_EXPECT(equal(da, send_amt));
            BEAST_EXPECT(equal(sa, A1["HKD"](10)));
            BEAST_EXPECT(same(
                st,
                stpath(G1, M1, G2),
                stpath(G1, M2, G2),
                stpath(G1, IPE(G2["HKD"]), G2),
                stpath(G1, IPE(zxcIssue()), IPE(G2["HKD"]), G2)));
        }
    }

    void
    path_find_06()
    {
        testcase("Path Find: non-ZXC -> non-ZXC, same currency)");
        using namespace jtx;
        Env env(*this);
        Account A1{"A1"};
        Account A2{"A2"};
        Account A3{"A3"};
        Account G1{"G1"};
        Account G2{"G2"};
        Account M1{"M1"};

        env.fund(ZXC(11000), M1);
        env.fund(ZXC(1000), A1, A2, A3, G1, G2);
        env.close();

        env.trust(G1["HKD"](2000), A1);
        env.trust(G2["HKD"](2000), A2);
        env.trust(A2["HKD"](2000), A3);
        env.trust(G1["HKD"](100000), M1);
        env.trust(G2["HKD"](100000), M1);
        env.close();

        env(pay(G1, A1, G1["HKD"](1000)));
        env(pay(G2, A2, G2["HKD"](1000)));
        env(pay(G1, M1, G1["HKD"](5000)));
        env(pay(G2, M1, G2["HKD"](5000)));
        env.close();

        env(offer(M1, G1["HKD"](1000), G2["HKD"](1000)));

        // E) Gateway to user
        //  Source -> OB -> AC -> Destination
        auto const& send_amt = A2["HKD"](10);
        STPathSet st;
        STAmount sa, da;
        std::tie(st, sa, da) =
            find_paths(env, G1, A2, send_amt, boost::none, G1["HKD"].currency);
        BEAST_EXPECT(equal(da, send_amt));
        BEAST_EXPECT(equal(sa, G1["HKD"](10)));
        BEAST_EXPECT(same(st, stpath(M1, G2), stpath(IPE(G2["HKD"]), G2)));
    }

    void
    run() override
    {
        source_currencies_limit();
        no_direct_path_no_intermediary_no_alternatives();
        direct_path_no_intermediary();
        payment_auto_path_find();
        path_find();
        path_find_consume_all();
        alternative_path_consume_both();
        alternative_paths_consume_best_transfer();
        alternative_paths_consume_best_transfer_first();
        alternative_paths_limit_returned_paths_to_best_quality();
        issues_path_negative_issue();
        issues_path_negative_ripple_client_issue_23_smaller();
        issues_path_negative_ripple_client_issue_23_larger();
        via_offers_via_gateway();
        indirect_paths_path_find();
        quality_paths_quality_set_and_test();
        trust_auto_clear_trust_normal_clear();
        trust_auto_clear_trust_auto_clear();
        zxc_to_zxc();

        // The following path_find_NN tests are data driven tests
        // that were originally implemented in js/coffee and migrated
        // here. The quantities and currencies used are taken directly from
        // those legacy tests, which in some cases probably represented
        // customer use cases.

        path_find_01();
        path_find_02();
        path_find_04();
        path_find_05();
        path_find_06();
    }
};

BEAST_DEFINE_TESTSUITE(Path, app, ripple);

}  // namespace test
}  // namespace ripple
