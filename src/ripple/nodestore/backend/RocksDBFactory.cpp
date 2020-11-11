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

<<<<<<< HEAD

=======
>>>>>>> release
#include <ripple/unity/rocksdb.h>

#if RIPPLE_ROCKSDB_AVAILABLE

#include <ripple/basics/ByteUtilities.h>
#include <ripple/basics/contract.h>
<<<<<<< HEAD
#include <ripple/basics/ByteUtilities.h>
#include <ripple/core/Config.h> // VFALCO Bad dependency
=======
#include <ripple/beast/core/CurrentThreadName.h>
#include <ripple/core/Config.h>  // VFALCO Bad dependency
>>>>>>> release
#include <ripple/nodestore/Factory.h>
#include <ripple/nodestore/Manager.h>
#include <ripple/nodestore/impl/BatchWriter.h>
#include <ripple/nodestore/impl/DecodedBlob.h>
#include <ripple/nodestore/impl/EncodedBlob.h>
#include <atomic>
#include <memory>

namespace ripple {
namespace NodeStore {

class RocksDBEnv : public rocksdb::EnvWrapper
{
public:
    RocksDBEnv() : EnvWrapper(rocksdb::Env::Default())
    {
    }

    struct ThreadParams
    {
        ThreadParams(void (*f_)(void*), void* a_) : f(f_), a(a_)
        {
        }

        void (*f)(void*);
        void* a;
    };

    static void
    thread_entry(void* ptr)
    {
        ThreadParams* const p(reinterpret_cast<ThreadParams*>(ptr));
        void (*f)(void*) = p->f;
        void* a(p->a);
        delete p;

        static std::atomic<std::size_t> n;
        std::size_t const id(++n);
        std::stringstream ss;
        ss << "rocksdb #" << id;
        beast::setCurrentThreadName(ss.str());

        (*f)(a);
    }

    void
<<<<<<< HEAD
    StartThread (void (*f)(void*), void* a) override
=======
    StartThread(void (*f)(void*), void* a) override
>>>>>>> release
    {
        ThreadParams* const p(new ThreadParams(f, a));
        EnvWrapper::StartThread(&RocksDBEnv::thread_entry, p);
    }
};

//------------------------------------------------------------------------------

class RocksDBBackend : public Backend, public BatchWriter::Callback
{
private:
    std::atomic<bool> m_deletePath;

public:
    beast::Journal m_journal;
    size_t const m_keyBytes;
    Scheduler& m_scheduler;
    BatchWriter m_batch;
    std::string m_name;
<<<<<<< HEAD
    std::unique_ptr <rocksdb::DB> m_db;
    int fdRequired_ = 2048;
    rocksdb::Options m_options;

    RocksDBBackend (int keyBytes, Section const& keyValues,
        Scheduler& scheduler, beast::Journal journal, RocksDBEnv* env)
        : m_deletePath (false)
        , m_journal (journal)
        , m_keyBytes (keyBytes)
        , m_scheduler (scheduler)
        , m_batch (*this, scheduler)
=======
    std::unique_ptr<rocksdb::DB> m_db;
    int fdRequired_ = 2048;
    rocksdb::Options m_options;

    RocksDBBackend(
        int keyBytes,
        Section const& keyValues,
        Scheduler& scheduler,
        beast::Journal journal,
        RocksDBEnv* env)
        : m_deletePath(false)
        , m_journal(journal)
        , m_keyBytes(keyBytes)
        , m_scheduler(scheduler)
        , m_batch(*this, scheduler)
>>>>>>> release
    {
        if (!get_if_exists(keyValues, "path", m_name))
            Throw<std::runtime_error>("Missing path in RocksDBFactory backend");

        rocksdb::BlockBasedTableOptions table_options;
        m_options.env = env;

<<<<<<< HEAD
        if (keyValues.exists ("cache_mb"))
            table_options.block_cache = rocksdb::NewLRUCache (
=======
        if (keyValues.exists("cache_mb"))
            table_options.block_cache = rocksdb::NewLRUCache(
>>>>>>> release
                get<int>(keyValues, "cache_mb") * megabytes(1));

        if (auto const v = get<int>(keyValues, "filter_bits"))
        {
<<<<<<< HEAD
            bool const filter_blocks = !keyValues.exists ("filter_full") ||
                (get<int>(keyValues, "filter_full") == 0);
            table_options.filter_policy.reset (rocksdb::NewBloomFilterPolicy (v, filter_blocks));
        }

        if (get_if_exists (keyValues, "open_files", m_options.max_open_files))
=======
            bool const filter_blocks = !keyValues.exists("filter_full") ||
                (get<int>(keyValues, "filter_full") == 0);
            table_options.filter_policy.reset(
                rocksdb::NewBloomFilterPolicy(v, filter_blocks));
        }

        if (get_if_exists(keyValues, "open_files", m_options.max_open_files))
>>>>>>> release
            fdRequired_ = m_options.max_open_files;

        if (keyValues.exists("file_size_mb"))
        {
<<<<<<< HEAD
            m_options.target_file_size_base = megabytes(1) * get<int>(keyValues,"file_size_mb");
            m_options.max_bytes_for_level_base = 5 * m_options.target_file_size_base;
            m_options.write_buffer_size = 2 * m_options.target_file_size_base;
        }

        get_if_exists (keyValues, "file_size_mult", m_options.target_file_size_multiplier);
=======
            m_options.target_file_size_base =
                megabytes(1) * get<int>(keyValues, "file_size_mb");
            m_options.max_bytes_for_level_base =
                5 * m_options.target_file_size_base;
            m_options.write_buffer_size = 2 * m_options.target_file_size_base;
        }

        get_if_exists(
            keyValues, "file_size_mult", m_options.target_file_size_multiplier);
>>>>>>> release

        if (keyValues.exists("bg_threads"))
        {
<<<<<<< HEAD
            m_options.env->SetBackgroundThreads
                (get<int>(keyValues, "bg_threads"), rocksdb::Env::LOW);
=======
            m_options.env->SetBackgroundThreads(
                get<int>(keyValues, "bg_threads"), rocksdb::Env::LOW);
>>>>>>> release
        }

        if (keyValues.exists("high_threads"))
        {
            auto const highThreads = get<int>(keyValues, "high_threads");
<<<<<<< HEAD
            m_options.env->SetBackgroundThreads (highThreads, rocksdb::Env::HIGH);
=======
            m_options.env->SetBackgroundThreads(
                highThreads, rocksdb::Env::HIGH);
>>>>>>> release

            // If we have high-priority threads, presumably we want to
            // use them for background flushes
            if (highThreads > 0)
                m_options.max_background_flushes = highThreads;
        }

        m_options.compression = rocksdb::kSnappyCompression;

        get_if_exists(keyValues, "block_size", table_options.block_size);

        if (keyValues.exists("universal_compaction") &&
            (get<int>(keyValues, "universal_compaction") != 0))
        {
            m_options.compaction_style = rocksdb::kCompactionStyleUniversal;
            m_options.min_write_buffer_number_to_merge = 2;
            m_options.max_write_buffer_number = 6;
            m_options.write_buffer_size = 6 * m_options.target_file_size_base;
<<<<<<< HEAD
        }

        if (keyValues.exists("bbt_options"))
        {
            auto const s = rocksdb::GetBlockBasedTableOptionsFromString(
                table_options,
                get<std::string>(keyValues, "bbt_options"),
                &table_options);
            if (! s.ok())
                Throw<std::runtime_error> (
                    std::string("Unable to set RocksDB bbt_options: ") + s.ToString());
        }

        m_options.table_factory.reset(NewBlockBasedTableFactory(table_options));

=======
        }

        if (keyValues.exists("bbt_options"))
        {
            auto const s = rocksdb::GetBlockBasedTableOptionsFromString(
                table_options,
                get<std::string>(keyValues, "bbt_options"),
                &table_options);
            if (!s.ok())
                Throw<std::runtime_error>(
                    std::string("Unable to set RocksDB bbt_options: ") +
                    s.ToString());
        }

        m_options.table_factory.reset(NewBlockBasedTableFactory(table_options));

>>>>>>> release
        if (keyValues.exists("options"))
        {
            auto const s = rocksdb::GetOptionsFromString(
                m_options, get<std::string>(keyValues, "options"), &m_options);
<<<<<<< HEAD
            if (! s.ok())
                Throw<std::runtime_error> (
                    std::string("Unable to set RocksDB options: ") + s.ToString());
=======
            if (!s.ok())
                Throw<std::runtime_error>(
                    std::string("Unable to set RocksDB options: ") +
                    s.ToString());
>>>>>>> release
        }

        std::string s1, s2;
        rocksdb::GetStringFromDBOptions(&s1, m_options, "; ");
        rocksdb::GetStringFromColumnFamilyOptions(&s2, m_options, "; ");
        JLOG(m_journal.debug()) << "RocksDB DBOptions: " << s1;
        JLOG(m_journal.debug()) << "RocksDB CFOptions: " << s2;
    }

<<<<<<< HEAD
    ~RocksDBBackend () override
=======
    ~RocksDBBackend() override
>>>>>>> release
    {
        close();
    }

    void
    open(bool createIfMissing) override
    {
        if (m_db)
        {
            assert(false);
<<<<<<< HEAD
            JLOG(m_journal.error()) <<
                "database is already open";
=======
            JLOG(m_journal.error()) << "database is already open";
>>>>>>> release
            return;
        }
        rocksdb::DB* db = nullptr;
        m_options.create_if_missing = createIfMissing;
        rocksdb::Status status = rocksdb::DB::Open(m_options, m_name, &db);
        if (!status.ok() || !db)
            Throw<std::runtime_error>(
                std::string("Unable to open/create RocksDB: ") +
                status.ToString());
        m_db.reset(db);
    }

    void
    close() override
    {
        if (m_db)
        {
            m_db.reset();
            if (m_deletePath)
            {
                boost::filesystem::path dir = m_name;
                boost::filesystem::remove_all(dir);
            }
        }
    }

    std::string
    getName() override
    {
        return m_name;
    }

    //--------------------------------------------------------------------------

    Status
    fetch(void const* key, std::shared_ptr<NodeObject>* pObject) override
    {
        assert(m_db);
<<<<<<< HEAD
        pObject->reset ();
=======
        pObject->reset();
>>>>>>> release

        Status status(ok);

        rocksdb::ReadOptions const options;
        rocksdb::Slice const slice(static_cast<char const*>(key), m_keyBytes);

        std::string string;

        rocksdb::Status getStatus = m_db->Get(options, slice, &string);

        if (getStatus.ok())
        {
            DecodedBlob decoded(key, string.data(), string.size());

            if (decoded.wasOk())
            {
                *pObject = decoded.createObject();
            }
            else
            {
                // Decoding failed, probably corrupted!
                //
                status = dataCorrupt;
            }
        }
        else
        {
            if (getStatus.IsCorruption())
            {
                status = dataCorrupt;
            }
            else if (getStatus.IsNotFound())
            {
                status = notFound;
            }
            else
            {
                status = Status(customCode + getStatus.code());

                JLOG(m_journal.error()) << getStatus.ToString();
            }
        }

        return status;
    }

    bool
    canFetchBatch() override
    {
        return false;
    }

    std::vector<std::shared_ptr<NodeObject>>
    fetchBatch(std::size_t n, void const* const* keys) override
    {
        Throw<std::runtime_error>("pure virtual called");
        return {};
    }

    void
    store(std::shared_ptr<NodeObject> const& object) override
    {
        m_batch.store(object);
    }

    void
    storeBatch(Batch const& batch) override
    {
        assert(m_db);
        rocksdb::WriteBatch wb;

        EncodedBlob encoded;

        for (auto const& e : batch)
        {
            encoded.prepare(e);

            wb.Put(
                rocksdb::Slice(
                    reinterpret_cast<char const*>(encoded.getKey()),
                    m_keyBytes),
                rocksdb::Slice(
                    reinterpret_cast<char const*>(encoded.getData()),
                    encoded.getSize()));
        }

        rocksdb::WriteOptions const options;

        auto ret = m_db->Write(options, &wb);

        if (!ret.ok())
            Throw<std::runtime_error>("storeBatch failed: " + ret.ToString());
    }

    void
    for_each(std::function<void(std::shared_ptr<NodeObject>)> f) override
    {
        assert(m_db);
        rocksdb::ReadOptions const options;

        std::unique_ptr<rocksdb::Iterator> it(m_db->NewIterator(options));

        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            if (it->key().size() == m_keyBytes)
            {
                DecodedBlob decoded(
                    it->key().data(), it->value().data(), it->value().size());

                if (decoded.wasOk())
                {
                    f(decoded.createObject());
                }
                else
                {
                    // Uh oh, corrupted data!
                    JLOG(m_journal.fatal())
                        << "Corrupt NodeObject #"
                        << from_hex_text<uint256>(it->key().data());
                }
            }
            else
            {
                // VFALCO NOTE What does it mean to find an
                //             incorrectly sized key? Corruption?
                JLOG(m_journal.fatal())
                    << "Bad key size = " << it->key().size();
            }
        }
    }

    int
    getWriteLoad() override
    {
        return m_batch.getWriteLoad();
    }

    void
    setDeletePath() override
    {
        m_deletePath = true;
    }

    //--------------------------------------------------------------------------

    void
    writeBatch(Batch const& batch) override
    {
        storeBatch(batch);
    }

    void
    verify() override
    {
    }

    /** Returns the number of file descriptors the backend expects to need */
    int
    fdRequired() const override
    {
        return fdRequired_;
    }
};

//------------------------------------------------------------------------------

class RocksDBFactory : public Factory
{
public:
    RocksDBEnv m_env;

    RocksDBFactory()
    {
        Manager::instance().insert(*this);
    }

<<<<<<< HEAD
    ~RocksDBFactory () override
=======
    ~RocksDBFactory() override
>>>>>>> release
    {
        Manager::instance().erase(*this);
    }

    std::string
<<<<<<< HEAD
    getName () const override
=======
    getName() const override
>>>>>>> release
    {
        return "RocksDB";
    }

    std::unique_ptr<Backend>
    createInstance(
        size_t keyBytes,
        Section const& keyValues,
        Scheduler& scheduler,
        beast::Journal journal) override
    {
        return std::make_unique<RocksDBBackend>(
            keyBytes, keyValues, scheduler, journal, &m_env);
    }
};

static RocksDBFactory rocksDBFactory;

}  // namespace NodeStore
}  // namespace ripple

#endif
