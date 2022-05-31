#pragma once

#include <chainsqlib/core/name.h>
#include <chainsqlib/core/datastream.h>
#include <chainsqlib/contracts/dispatcher.h>

namespace chainsql
{
    class contract
    {
    public:
        contract(){}

        contract(name self, name first_receiver, datastream<const char *> ds)
            : self_(self), first_receiver_(first_receiver), ds_(ds)
        {
        }

        /**
         *
         * Get this contract name
         *
         * @return name - The name of this contract
         */
        inline name get_self() const { return self_; }

        /**
         * The account the incoming action was first received at.
         *
         * @return name - The first_receiver name of the action this contract is processing.
         */
        inline name get_first_receiver() const { return first_receiver_; }

        /**
         * Get the datastream for this contract
         *
         * @return datastream<const char*> - The datastream for this contract
         */
        inline datastream<const char *> &get_datastream() { return ds_; }

        /**
         * Get the datastream for this contract
         *
         * @return datastream<const char*> - The datastream for this contract
         */
        inline const datastream<const char *> &get_datastream() const { return ds_; }

    private:
        /**
         * The name of the account this contract is deployed on.
         */
        name self_;

        /**
         * The account the incoming action was first received at.
         */
        name first_receiver_;

        /**
         * The datastream for this contract
         */
        datastream<const char *> ds_ = datastream<const char *>(nullptr, 0);
    };
} // namespace chainsql