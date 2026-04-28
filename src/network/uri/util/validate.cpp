#include <string>
#include <regex>
#include <algorithm>
#include <cctype>
#include <idn2.h>

#include "utilURI.h"

// ---- IDN → ASCII using libidn2 ----
std::string toASCII_IDN(const std::string &input) {
    char *output = nullptr;

    // IDN2_NONTRANSITIONAL is recommended for modern behavior
    int rc = idn2_to_ascii_8z(input.c_str(), &output, IDN2_NONTRANSITIONAL);

    if (rc != IDN2_OK || output == nullptr) {
        return {}; // signal failure
    }

    std::string result(output);
    idn2_free(output);
    return result;
}

bool isAllDigits(const std::string &s) {
    return !s.empty() &&
           std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isdigit(c); });
}

bool uri::validate::Host::validateHost(std::string host) {
    if (host.empty())
        return false;

    // ---- Normalize case ----
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // ---- Remove trailing dot (FQDN) ----
    if (!host.empty() && host.back() == '.') {
        host.pop_back();
    }

    if (host.empty())
        return false;

    // ---- Localhost ----
    if (host == "localhost")
        return true;

    // ---- IDN conversion ----
    host = toASCII_IDN(host);
    if (host.empty())
        return false;

    // ---- Length check ----
    if (host.size() > 253)
        return false;

    // ---- Regex (structure) ----
    static const std::regex pattern(
        R"(^(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)*[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$)",
        std::regex::ECMAScript
    );

    if (!std::regex_match(host, pattern))
        return false;

    // ---- Require at least one dot ----
    auto lastDot = host.rfind('.');
    if (lastDot == std::string::npos)
        return false;

    std::string tld = host.substr(lastDot + 1);

    // ---- TLD checks ----
    if (tld.size() < 2)
        return false;

    if (isAllDigits(tld))
        return false;

    // Optional blacklist
    //static const std::string invalidTlds[] = {
    //    "invalid", "test", "example", "localhost"
    //};
    static const std::vector<std::string> invalidTlds{"invalid", "test", "example", "localhost"};

    for (const auto &bad: invalidTlds) {
        if (tld == bad)
            return false;
    }

    return true;
}


bool uri::validate::Host::validateIP(std::string host) {
    // RegEx from https://jsfiddle.net/opd1v7au/2/
    static const std::regex r(
        R"/((^\s*((([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]))\s*$)|(^\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\s*$))/");
    return std::regex_match(host, r);
}
