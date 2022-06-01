#pragma once

#include <string>
#include <any>
#include <vector>
#include <memory>

#include "chainsqlib/core/table/condition.h"
#include "chainsqlib/core/table/column.h"
#include "chainsqlib/core/table/table.h"

namespace chainsql {

    class BuildTable
    {
    public:
        template <typename T>
        static T New(const std::string &tableName)
        {
            return T(tableName);
        }
    };
}