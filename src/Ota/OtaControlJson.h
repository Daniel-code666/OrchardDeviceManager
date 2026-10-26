#pragma once

#include <string>
#include "OtaTypes.h"

class OtaControlJson
{
public:
    static bool TryParseUpdateRequest(const std::string &json, std::string &manifestUrl);
    static bool DeserializeRemoteRequest(const std::string &json, RemoteOtaRequest &request);
};

