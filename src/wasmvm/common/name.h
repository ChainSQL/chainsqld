#pragma once

#include <string>
#include <string_view>

#include "check.h"

namespace chainsql
{

    struct name
    {
    public:
        enum class raw : uint64_t {};

        /**
         * Construct a new name
         *
         * @brief Construct a new name object defaulting to a value of 0
         *
         */
        constexpr name() : value(0) {}

        /**
         * Construct a new name given a unit64_t value
         *
         * @brief Construct a new name object initialising value with v
         * @param v - The unit64_t value
         *
         */
        constexpr explicit name(uint64_t v)
            : value(v)
        {
        }

        /**
         * Construct a new name given a scoped enumerated type of raw (uint64_t).
         *
         * @brief Construct a new name object initialising value with r
         * @param r - The raw value which is a scoped enumerated type of unit64_t
         *
         */
        constexpr explicit name(name::raw r)
            : value(static_cast<uint64_t>(r))
        {
        }

        constexpr explicit name(std::string_view str)
            : value(0)
        {
            if (str.size() > 13)
            {
                chainsql::check(false, "string is too long to be a valid name");
            }
            if (str.empty())
            {
                return;
            }

            auto n = std::min((uint32_t)str.size(), (uint32_t)12u);
            for (decltype(n) i = 0; i < n; ++i)
            {
                value <<= 5;
                value |= char_to_value(str[i]);
            }
            value <<= (4 + 5 * (12 - n));
            if (str.size() == 13)
            {
                uint64_t v = char_to_value(str[12]);
                if (v > 0x0Full)
                {
                    chainsql::check(false, "thirteenth character in name cannot be a letter that comes after j");
                }
                value |= v;
            }
        }

        /**
         *  Returns the name as a string.
         *
         *  @brief Returns the name value as a string by calling write_as_string() and returning the buffer produced by write_as_string()
         */
        std::string to_string() const
        {
            char buffer[13];
            auto end = write_as_string(buffer, buffer + sizeof(buffer));
            return {buffer, end};
        }

        /**
         * Casts a name to raw
         *
         * @return Returns an instance of raw based on the value of a name
         */
        constexpr operator raw() const { return raw(value); }

        /**
         * Explicit cast to bool of the uint64_t value of the name
         *
         * @return Returns true if the name is set to the default value of 0 else true.
         */
        constexpr explicit operator bool() const { return value != 0; }

        /**
         * Equivalency operator. Returns true if a == b (are the same)
         *
         * @return boolean - true if both provided %name values are the same
         */
        friend constexpr bool operator==(const name &a, const name &b)
        {
            return a.value == b.value;
        }

        /**
         * Inverted equivalency operator. Returns true if a != b (are different)
         *
         * @return boolean - true if both provided %name values are not the same
         */
        friend constexpr bool operator!=(const name &a, const name &b)
        {
            return a.value != b.value;
        }

        /**
         * Less than operator. Returns true if a < b.
         *
         * @return boolean - true if %name `a` is less than `b`
         */
        friend constexpr bool operator<(const name &a, const name &b)
        {
            return a.value < b.value;
        }

        uint64_t value = 0;
    private:
        /**
         *  Converts a %name Base32 symbol into its corresponding value
         *
         *  @param c - Character to be converted
         *  @return constexpr char - Converted value
         */
        static constexpr uint8_t char_to_value(char c)
        {
            if (c == '.')
                return 0;
            else if (c >= '1' && c <= '5')
                return (c - '1') + 1;
            else if (c >= 'a' && c <= 'z')
                return (c - 'a') + 6;
            else
                chainsql::check(false, "character is not in allowed character set for names");

            return 0; // control flow will never reach here; just added to suppress warning
        }

        /**
         *  Returns the length of the %name
         */
        constexpr uint8_t length() const
        {
            constexpr uint64_t mask = 0xF800000000000000ull;

            if (value == 0)
                return 0;

            uint8_t l = 0;
            uint8_t i = 0;
            for (auto v = value; i < 13; ++i, v <<= 5)
            {
                if ((v & mask) > 0)
                {
                    l = i;
                }
            }

            return l + 1;
        }

        /**
         *  Writes the %name as a string to the provided char buffer
         *
         *  @pre The range [begin, end) must be a valid range of memory to write to.
         *  @param begin - The start of the char buffer
         *  @param end - Just past the end of the char buffer
         *  @param dry_run - If true, do not actually write anything into the range.
         *  @return char* - Just past the end of the last character that would be written assuming dry_run == false and end was large enough to provide sufficient space. (Meaning only applies if returned pointer >= begin.)
         *  @post If the output string fits within the range [begin, end) and dry_run == false, the range [begin, returned pointer) contains the string representation of the %name. Nothing is written if dry_run == true or returned pointer > end (insufficient space) or if returned pointer < begin (overflow in calculating desired end).
         */
        char *write_as_string(char *begin, char *end, bool dry_run = false) const
        {
            static const char *charmap = ".12345abcdefghijklmnopqrstuvwxyz";
            constexpr uint64_t mask = 0xF800000000000000ull;

            if (dry_run || (begin + 13 < begin) || (begin + 13 > end))
            {
                char *actual_end = begin + length();
                if (dry_run || (actual_end < begin) || (actual_end > end))
                    return actual_end;
            }

            auto v = value;
            for (auto i = 0; i < 13; ++i, v <<= 5)
            {
                if (v == 0)
                    return begin;

                auto indx = (v & mask) >> (i == 12 ? 60 : 59);
                *begin = charmap[indx];
                ++begin;
            }

            return begin;
        }
    };

} // namespace chainsql

#define N(exp) chainsql::name(#exp).value
