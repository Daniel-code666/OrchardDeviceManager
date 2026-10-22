#pragma once

#include <string>

class OtaControlJson
{
public:
    static bool TryParseUpdateRequest(const std::string &json, std::string &manifestUrl);
};

