// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////
#include <utilities/String.h>
/////////////////////////////

#include <algorithm>
#include <sstream>

namespace Utilities
{
    /* Erases all instances of c from the string. E.g. 2,000,000 becomes 2000000 */
    void removeCharFromString(std::string &str, const char c)
    {
        str.erase(std::remove(str.begin(), str.end(), c), str.end());
    }

    /* Trims any whitespace from left and right */
    void trim(std::string &str)
    {
        rightTrim(str);
        leftTrim(str);
    }

    void leftTrim(std::string &str)
    {
        std::string whitespace = " \t\n\r\f\v";

        str.erase(0, str.find_first_not_of(whitespace));
    }

    void rightTrim(std::string &str)
    {
        std::string whitespace = " \t\n\r\f\v";

        str.erase(str.find_last_not_of(whitespace) + 1);
    }

    /* Checks if str begins with substring */
    bool startsWith(const std::string &str, const std::string &substring)
    {
        return str.rfind(substring, 0) == 0;
    }

    std::vector<std::string> split(const std::string &str, char delimiter = ' ')
    {
        std::vector<std::string> cont;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter))
        {
            cont.push_back(token);
        }

        return cont;
    }

    std::string removePrefix(const std::string &str, const std::string &prefix)
    {
        const size_t removePos = str.rfind(prefix, 0);

        if (removePos == 0)
        {
            /* erase is in place */
            std::string copy = str;

            copy.erase(0, prefix.length());

            return copy;
        }

        return str;
    }

    /* Replaces every occurrence of 'from' with 'to', in place. Matches
       boost::replace_all: matches do not overlap, and scanning resumes after
       the inserted text so 'to' is never rescanned. */
    void replaceAll(std::string &str, const std::string &from, const std::string &to)
    {
        if (from.empty())
        {
            return;
        }

        size_t pos = 0;

        while ((pos = str.find(from, pos)) != std::string::npos)
        {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

} // namespace Utilities
