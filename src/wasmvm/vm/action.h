#pragma once

#include "chainsqlib/core/name.h"

namespace chainsql {

    class action {
    private:
        name code_;
        name function_;
        std::string payload_;
    public:
        action(
            const name &code,
            const name &func,
            const std::string &payload)
            : code_(code), function_(func), payload_(payload)
        {
        }

        ~action()
        {
        }

        const name& code() const
        {
            return code_;
        }

        const name& function() const
        {
            return function_;
        }

        const std::string& payload() const
        {
            return payload_;
        }
    };
}
