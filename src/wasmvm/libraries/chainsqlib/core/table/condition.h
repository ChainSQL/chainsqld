#pragma once

namespace chainsql {namespace Table{namespace Condition {
    template <typename T>
    class expression
    {
    public:
        expression(const std::string &n, const T &v)
            : name(n), value(v)
        {
        }

    protected:
        std::string name;
        T value;
    };

    template <typename T>
    class eq : public expression<T>
    {
    public:
        eq(const std::string &n, const T &v)
            : expression<T>(n, v)
        {
        }
    };
}}}