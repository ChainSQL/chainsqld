#pragma once

#include <string>

#define NAME
#define DEFINITION
#define NOTNULL
#define KEY
#define UNIQUE
#define INDEX
#define AUTOCREMENT
#define DEFUALTVALUE
#define COLUMNVALUE

using ColumnName = std::string;

namespace chainsql
{
    template <typename T>
    struct ColumnDescription
    {
        bool isNull;
        bool isKey;
        bool isUnique;
        bool isIndex;
        bool isAutoIncrement;
        T defaultValue;
    };

    template <typename T>
    struct ColumnDefiniton
    {
        ColumnName name;
        ColumnDescription<T> definition;
    };

    template <typename T>
    struct ColunmValue {
        ColumnName name;
        T value;
    };
}