// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "StringTools.h"

#include <algorithm>
#include <fstream>
#include <iomanip>

namespace Common
{
    namespace
    {
        const uint8_t characterValues[256] = {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
            0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff};

    }

    std::string asString(const void *data, uint64_t size)
    {
        return std::string(static_cast<const char *>(data), size);
    }

    std::string asString(const std::vector<uint8_t> &data)
    {
        return std::string(reinterpret_cast<const char *>(data.data()), data.size());
    }

    std::vector<uint8_t> asBinaryArray(const std::string &data)
    {
        auto dataPtr = reinterpret_cast<const uint8_t *>(data.data());
        return std::vector<uint8_t>(dataPtr, dataPtr + data.size());
    }

    uint8_t fromHex(char character)
    {
        uint8_t value = characterValues[static_cast<unsigned char>(character)];
        if (value > 0x0f)
        {
            throw std::runtime_error("fromHex: invalid character");
        }

        return value;
    }

    bool fromHex(char character, uint8_t &value)
    {
        if (characterValues[static_cast<unsigned char>(character)] > 0x0f)
        {
            return false;
        }

        value = characterValues[static_cast<unsigned char>(character)];
        return true;
    }

    uint64_t fromHex(const std::string &text, void *data, uint64_t bufferSize)
    {
        if ((text.size() & 1) != 0)
        {
            throw std::runtime_error("fromHex: invalid string size");
        }

        if (text.size() >> 1 > bufferSize)
        {
            throw std::runtime_error("fromHex: invalid buffer size");
        }

        for (uint64_t i = 0; i<text.size()>> 1; ++i)
        {
            static_cast<uint8_t *>(data)[i] = fromHex(text[i << 1]) << 4 | fromHex(text[(i << 1) + 1]);
        }

        return text.size() >> 1;
    }

    bool fromHex(const std::string &text, void *data, uint64_t bufferSize, uint64_t &size)
    {
        if ((text.size() & 1) != 0)
        {
            return false;
        }

        if (text.size() >> 1 > bufferSize)
        {
            return false;
        }

        for (uint64_t i = 0; i<text.size()>> 1; ++i)
        {
            uint8_t value1;
            if (!fromHex(text[i << 1], value1))
            {
                return false;
            }

            uint8_t value2;
            if (!fromHex(text[(i << 1) + 1], value2))
            {
                return false;
            }

            static_cast<uint8_t *>(data)[i] = value1 << 4 | value2;
        }

        size = text.size() >> 1;
        return true;
    }

    std::vector<uint8_t> fromHex(const std::string &text)
    {
        if ((text.size() & 1) != 0)
        {
            throw std::runtime_error("fromHex: invalid string size");
        }

        std::vector<uint8_t> data(text.size() >> 1);
        for (uint64_t i = 0; i < data.size(); ++i)
        {
            data[i] = fromHex(text[i << 1]) << 4 | fromHex(text[(i << 1) + 1]);
        }

        return data;
    }

    bool fromHex(const std::string &text, std::vector<uint8_t> &data)
    {
        if ((text.size() & 1) != 0)
        {
            return false;
        }

        for (uint64_t i = 0; i<text.size()>> 1; ++i)
        {
            uint8_t value1;
            if (!fromHex(text[i << 1], value1))
            {
                return false;
            }

            uint8_t value2;
            if (!fromHex(text[(i << 1) + 1], value2))
            {
                return false;
            }

            data.push_back(value1 << 4 | value2);
        }

        return true;
    }

    std::string toHex(const void *data, uint64_t size)
    {
        std::string text;
        for (uint64_t i = 0; i < size; ++i)
        {
            text += "0123456789abcdef"[static_cast<const uint8_t *>(data)[i] >> 4];
            text += "0123456789abcdef"[static_cast<const uint8_t *>(data)[i] & 15];
        }

        return text;
    }

    void toHex(const void *data, uint64_t size, std::string &text)
    {
        for (uint64_t i = 0; i < size; ++i)
        {
            text += "0123456789abcdef"[static_cast<const uint8_t *>(data)[i] >> 4];
            text += "0123456789abcdef"[static_cast<const uint8_t *>(data)[i] & 15];
        }
    }

    std::string toHex(const std::vector<uint8_t> &data)
    {
        std::string text;
        for (uint64_t i = 0; i < data.size(); ++i)
        {
            text += "0123456789abcdef"[data[i] >> 4];
            text += "0123456789abcdef"[data[i] & 15];
        }

        return text;
    }

    void toHex(const std::vector<uint8_t> &data, std::string &text)
    {
        for (uint64_t i = 0; i < data.size(); ++i)
        {
            text += "0123456789abcdef"[data[i] >> 4];
            text += "0123456789abcdef"[data[i] & 15];
        }
    }

    std::string toBase64(const void *data, uint64_t size)
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        const auto *bytes = static_cast<const uint8_t *>(data);

        std::string text;
        text.reserve(((size + 2) / 3) * 4);

        uint64_t i = 0;

        for (; i + 3 <= size; i += 3)
        {
            const uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16)
                                    | (static_cast<uint32_t>(bytes[i + 1]) << 8)
                                    | static_cast<uint32_t>(bytes[i + 2]);

            text += alphabet[(triple >> 18) & 0x3f];
            text += alphabet[(triple >> 12) & 0x3f];
            text += alphabet[(triple >> 6) & 0x3f];
            text += alphabet[triple & 0x3f];
        }

        const uint64_t remaining = size - i;

        if (remaining == 1)
        {
            const uint32_t triple = static_cast<uint32_t>(bytes[i]) << 16;

            text += alphabet[(triple >> 18) & 0x3f];
            text += alphabet[(triple >> 12) & 0x3f];
            text += '=';
            text += '=';
        }
        else if (remaining == 2)
        {
            const uint32_t triple =
                (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8);

            text += alphabet[(triple >> 18) & 0x3f];
            text += alphabet[(triple >> 12) & 0x3f];
            text += alphabet[(triple >> 6) & 0x3f];
            text += '=';
        }

        return text;
    }

    std::string toBase64(const std::vector<uint8_t> &data)
    {
        return toBase64(data.data(), data.size());
    }

    namespace
    {
        /* 0xff for anything that is not a base64 digit, so a single table
           lookup rejects stray characters as well as decoding valid ones. */
        int8_t base64Value(char character)
        {
            if (character >= 'A' && character <= 'Z')
            {
                return static_cast<int8_t>(character - 'A');
            }

            if (character >= 'a' && character <= 'z')
            {
                return static_cast<int8_t>(character - 'a' + 26);
            }

            if (character >= '0' && character <= '9')
            {
                return static_cast<int8_t>(character - '0' + 52);
            }

            if (character == '+')
            {
                return 62;
            }

            if (character == '/')
            {
                return 63;
            }

            return -1;
        }

        bool decodeBase64(const std::string &text, std::vector<uint8_t> &out)
        {
            /* Padded base64 only - the encoder above always pads, and refusing
               anything else keeps a truncated response from decoding into a
               short but plausible looking key. */
            if (text.size() % 4 != 0)
            {
                return false;
            }

            out.reserve(out.size() + (text.size() / 4) * 3);

            for (uint64_t i = 0; i < text.size(); i += 4)
            {
                const bool padThird = text[i + 2] == '=';
                const bool padFourth = text[i + 3] == '=';

                /* Padding only ever comes at the very end, and '=' in the third
                   position means the fourth must be padding too. */
                if ((padThird || padFourth) && i + 4 != text.size())
                {
                    return false;
                }

                if (padThird && !padFourth)
                {
                    return false;
                }

                const int8_t first = base64Value(text[i]);
                const int8_t second = base64Value(text[i + 1]);
                const int8_t third = padThird ? 0 : base64Value(text[i + 2]);
                const int8_t fourth = padFourth ? 0 : base64Value(text[i + 3]);

                if (first < 0 || second < 0 || third < 0 || fourth < 0)
                {
                    return false;
                }

                const uint32_t triple = (static_cast<uint32_t>(first) << 18)
                                        | (static_cast<uint32_t>(second) << 12)
                                        | (static_cast<uint32_t>(third) << 6) | static_cast<uint32_t>(fourth);

                out.push_back(static_cast<uint8_t>((triple >> 16) & 0xff));

                if (!padThird)
                {
                    out.push_back(static_cast<uint8_t>((triple >> 8) & 0xff));
                }

                if (!padFourth)
                {
                    out.push_back(static_cast<uint8_t>(triple & 0xff));
                }
            }

            return true;
        }
    } // namespace

    bool fromBase64(const std::string &text, void *data, uint64_t bufferSize, uint64_t &size)
    {
        std::vector<uint8_t> decoded;

        if (!decodeBase64(text, decoded))
        {
            return false;
        }

        if (decoded.size() > bufferSize)
        {
            return false;
        }

        std::copy(decoded.begin(), decoded.end(), static_cast<uint8_t *>(data));

        size = decoded.size();

        return true;
    }

    bool fromBase64(const std::string &text, std::vector<uint8_t> &data)
    {
        return decodeBase64(text, data);
    }

    std::string extract(std::string &text, char delimiter)
    {
        uint64_t delimiterPosition = text.find(delimiter);
        std::string subText;
        if (delimiterPosition != std::string::npos)
        {
            subText = text.substr(0, delimiterPosition);
            text = text.substr(delimiterPosition + 1);
        }
        else
        {
            subText.swap(text);
        }

        return subText;
    }

    std::string extract(const std::string &text, char delimiter, uint64_t &offset)
    {
        uint64_t delimiterPosition = text.find(delimiter, offset);
        if (delimiterPosition != std::string::npos)
        {
            offset = delimiterPosition + 1;
            return text.substr(offset, delimiterPosition);
        }
        else
        {
            offset = text.size();
            return text.substr(offset);
        }
    }

    std::string ipAddressToString(uint32_t ip)
    {
        uint8_t bytes[4];
        bytes[0] = ip & 0xFF;
        bytes[1] = (ip >> 8) & 0xFF;
        bytes[2] = (ip >> 16) & 0xFF;
        bytes[3] = (ip >> 24) & 0xFF;

        char buf[16];
        sprintf(buf, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);

        return std::string(buf);
    }

    bool parseIpAddressAndPort(uint32_t &ip, uint32_t &port, const std::string &addr)
    {
        uint32_t v[4];
        uint32_t localPort;

        if (sscanf(addr.c_str(), "%d.%d.%d.%d:%d", &v[0], &v[1], &v[2], &v[3], &localPort) != 5)
        {
            return false;
        }

        for (int i = 0; i < 4; ++i)
        {
            if (v[i] > 0xff)
            {
                return false;
            }
        }

        ip = (v[3] << 24) | (v[2] << 16) | (v[1] << 8) | v[0];
        port = localPort;
        return true;
    }

    bool parseHostAndPort(const std::string &addr, std::string &host, uint32_t &port)
    {
        if (addr.empty())
        {
            return false;
        }

        if (addr[0] == '[')
        {
            // IPv6 bracket notation: "[::1]:8080"
            auto closeBracket = addr.find(']');
            if (closeBracket == std::string::npos)
            {
                return false;
            }
            host = addr.substr(1, closeBracket - 1);
            if (closeBracket + 1 >= addr.size() || addr[closeBracket + 1] != ':')
            {
                return false;
            }
            try
            {
                port = static_cast<uint32_t>(std::stoul(addr.substr(closeBracket + 2)));
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        // IPv4 or hostname: "1.2.3.4:8080" or "hostname:8080"
        auto colon = addr.rfind(':');
        if (colon == std::string::npos)
        {
            return false;
        }
        host = addr.substr(0, colon);
        try
        {
            port = static_cast<uint32_t>(std::stoul(addr.substr(colon + 1)));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    std::string timeIntervalToString(uint64_t intervalInSeconds)
    {
        auto tail = intervalInSeconds;

        auto days = tail / (60 * 60 * 24);
        tail = tail % (60 * 60 * 24);
        auto hours = tail / (60 * 60);
        tail = tail % (60 * 60);
        auto minutes = tail / (60);
        tail = tail % (60);
        auto seconds = tail;

        std::stringstream ss;
        ss << "d" << days << std::setfill('0') << ".h" << std::setw(2) << hours << ".m" << std::setw(2) << minutes
           << ".s" << std::setw(2) << seconds;

        return ss.str();
    }

} // namespace Common
