#pragma once

#include <string>
#include <any>
#include <vector>
#include <map>

#include "chainsqlib/core/table/column.h"

namespace chainsql
{
    namespace Table
    {
        namespace internal_use_do_not_use
        {
            extern "C"
            {
                int32_t create_table(const void *data, int32_t size);
                int32_t update_table(const void *data, int32_t size);
                int32_t delete_table(const void *data, int32_t size);
                int32_t rename_table(const void *data, int32_t size);
                int32_t drop_table(const void *data, int32_t size);
                int32_t grant_table(const void *data, int32_t size);
                int32_t query_table(const void *data, int32_t size);
                int32_t record_lines(int32_t handle);
                int32_t get_colunms(int32_t handle);
                int32_t get_colunm_value(int32_t handle,
                                         int32_t record_index, int32_t col_index,
                                         void *col_buffer, int32_t *col_buf_size,
                                         void *value_buffer, int32_t *value_buf_size);

                void transaction_begin();
                void transaction_commit();
            }
        }

        class Create
        {
        public:
            Create(const std::string &name)
                : name_(name) {}

            template <typename T>
            Create &addColumn(const T &column)
            {
                columns_.push_back(column);
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::create_table(this, sizeof(*this));
            }

        private:
            std::string name_;
            std::vector<std::any> columns_;
        };

        using Insert = Create;

        class Update
        {
        public:
            Update(const std::string &name)
                : name_(name) {}

            template <typename T>
            Update &addColumn(const ColunmValue<T> &column)
            {
                columns_.push_back(column);
                return *this;
            }

            Update &withCondition()
            {
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::update_table(this, sizeof(*this));
            }

        private:
            std::string name_;
            std::vector<std::any> columns_;
        };

        class Delete
        {
        public:
            Delete(const std::string &name)
                : name_(name) {}

            Delete &withCondition()
            {
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::delete_table(this, sizeof(*this));
            }

        private:
            std::string name_;
        };

        class Rename 
        {
        public:
            Rename(const std::string &name)
                : old_(name) {}

            Rename &newName(const std::string &name)
            {
                new_ = name;
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::rename_table(this, sizeof(*this));
            }

        private:
            std::string old_;
            std::string new_;
        };

        class Drop
        {
        public:
            Drop(const std::string &name)
                : name_(name) {}

            int32_t execute()
            {
                return internal_use_do_not_use::drop_table(this, sizeof(*this));
            }

        private:
            std::string name_;
        };

        class Grant
        {
        public:
            enum
            {
                SELECT = 0x01,
                INSERT = 0x02,
                UPDATE = 0x04,
                DELETE = 0x08
            };

            Grant(const std::string &name)
                : name_(name) {}

            Grant &grant(const std::string &address, int permission)
            {
                permissions_[address] = permission;
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::grant_table(this, sizeof(*this));
            }

        private:
            std::string name_;
            std::map<std::string, int> permissions_;
        };

        class Query
        {
        public:
            Query(const std::string &name)
                : name_(name) {}

            Query &addColumn(const ColumnName &column)
            {
                colunms_.push_back(column);
                return *this;
            }

            Query &withCondition()
            {
                return *this;
            }

            int32_t execute()
            {
                return internal_use_do_not_use::query_table(this, sizeof(*this));
            }

        private:
            std::string name_;
            std::vector<ColumnName> colunms_;
        };

        class QueryResult
        {
        public:
            using OneRecord = std::vector<std::any>;
            QueryResult(const int32_t &handle)
                : handle_(handle)
                , counts_(internal_use_do_not_use::record_lines(handle))
                , index_(0) {}

            //ColunmValue next()
            //{
            //    OneRecord one;

            //    return one;
            //}

        private:
            int32_t handle_;
            int32_t counts_;
            int32_t index_;
        };

        namespace Transaction
        {
            void begin()
            {
                internal_use_do_not_use::transaction_begin();
            }

            void commit()
            {
                internal_use_do_not_use::transaction_commit();
            }
        }
    }
}