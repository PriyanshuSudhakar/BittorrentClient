#pragma once

#include <string>
#include "lib/nlohmann/json.hpp"

namespace Bencode
{
    // Declares the decoding function
    nlohmann::json decode_bencoded_value(const std::string &encoded_value);

    // Declares the encoding function
    std::string json_to_bencode(const nlohmann::json &value);
}