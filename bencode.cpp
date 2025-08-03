#include "bencode.h"
#include <cctype>
#include <stdexcept>
#include <sstream>

// Anonymous namespace to keep helper functions private to this file
namespace
{
    using json = nlohmann::json;

    // Forward declare the main recursive function
    json decode_recursive(const std::string &encoded_value, size_t &index);

    json decode_string(const std::string &encoded_value, size_t &index)
    {
        size_t colon_pos = encoded_value.find(':', index);
        if (colon_pos == std::string::npos)
        {
            throw std::runtime_error("Invalid bencoded string: missing colon.");
        }

        long long length = std::stoll(encoded_value.substr(index, colon_pos - index));
        index = colon_pos + 1;

        if (index + length > encoded_value.size())
        {
            throw std::runtime_error("Invalid bencoded string: length exceeds buffer size.");
        }

        std::string result = encoded_value.substr(index, length);
        index += length;
        return result;
    }

    json decode_integer(const std::string &encoded_value, size_t &index)
    {
        index++; // Skip 'i'
        size_t end_pos = encoded_value.find('e', index);
        if (end_pos == std::string::npos)
        {
            throw std::runtime_error("Invalid bencoded integer: missing 'e'.");
        }

        long long value = std::stoll(encoded_value.substr(index, end_pos - index));
        index = end_pos + 1;
        return json(value);
    }

    json decode_list(const std::string &encoded_value, size_t &index)
    {
        index++; // Skip 'l'
        json list = json::array();
        while (index < encoded_value.size() && encoded_value[index] != 'e')
        {
            list.push_back(decode_recursive(encoded_value, index));
        }
        if (index >= encoded_value.size())
        {
            throw std::runtime_error("Invalid bencoded list: missing 'e'.");
        }
        index++; // Skip 'e'
        return list;
    }

    json decode_dict(const std::string &encoded_value, size_t &index)
    {
        index++; // Skip 'd'
        json dict = json::object();
        while (index < encoded_value.size() && encoded_value[index] != 'e')
        {
            json key = decode_string(encoded_value, index);
            json value = decode_recursive(encoded_value, index);
            dict[key.get<std::string>()] = value;
        }
        if (index >= encoded_value.size())
        {
            throw std::runtime_error("Invalid bencoded dictionary: missing 'e'.");
        }
        index++; // Skip 'e'
        return dict;
    }

    // This is the main recursive dispatcher
    json decode_recursive(const std::string &encoded_value, size_t &index)
    {
        if (std::isdigit(encoded_value[index]))
        {
            return decode_string(encoded_value, index);
        }
        else if (encoded_value[index] == 'i')
        {
            return decode_integer(encoded_value, index);
        }
        else if (encoded_value[index] == 'l')
        {
            return decode_list(encoded_value, index);
        }
        else if (encoded_value[index] == 'd')
        {
            return decode_dict(encoded_value, index);
        }
        else
        {
            throw std::runtime_error("Unhandled bencoded value type.");
        }
    }

} // End of anonymous namespace

// --- Implementation of the public functions from bencode.h ---
namespace Bencode
{
    // FIX: The function name now matches the header declaration
    nlohmann::json decode_bencoded_value(const std::string &encoded_value)
    {
        size_t index = 0;
        nlohmann::json result = decode_recursive(encoded_value, index);

        if (index != encoded_value.size())
        {
            throw std::runtime_error("Bencode string not fully consumed. Extra data at end.");
        }

        return result;
    }

    // FIX: This function is now correctly placed inside the Bencode namespace
    std::string json_to_bencode(const nlohmann::json &js)
    {
        std::ostringstream os;
        if (js.is_object())
        {
            os << 'd';
            for (auto &el : js.items())
            {
                os << el.key().size() << ':' << el.key();
                // The recursive call must also use the full, original name
                os << Bencode::json_to_bencode(el.value());
            }
            os << 'e';
        }
        else if (js.is_array())
        {
            os << 'l';
            for (const auto &item : js)
            {
                // The recursive call must also use the full, original name
                os << Bencode::json_to_bencode(item);
            }
            os << 'e';
        }
        else if (js.is_number_integer())
        {
            os << 'i' << js.get<long long>() << 'e';
        }
        else if (js.is_string())
        {
            const std::string &value = js.get<std::string>();
            os << value.size() << ':' << value;
        }
        return os.str();
    }
}
