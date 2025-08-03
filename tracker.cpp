#include "tracker.h"
#include "torrent_file.h"
#include "bencode.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream> // Added for debugging output

#include "curl/curl.h"

namespace
{
    // This is a C-style callback function required by libcurl.
    size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
    {
        userp->append(static_cast<char *>(contents), size * nmemb);
        return size * nmemb;
    }

    // Helper to URL-encode the binary info hash and peer ID.
    std::string urlEncode(const std::string &value)
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (unsigned char c : value)
        {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped << c;
            }
            else
            {
                escaped << '%' << std::setw(2) << static_cast<int>(c);
            }
        }
        return escaped.str();
    }
}

std::vector<std::string> Tracker::getPeers(const TorrentFile &torrent, const std::string &peerId, uint16_t port) const
{
    // 1. Build the tracker URL
    std::ostringstream url;
    url << torrent.getTrackerUrl()
        << "?info_hash=" << urlEncode(torrent.getInfoHashBinary())
        << "&peer_id=" << urlEncode(peerId)
        << "&port=" << port
        << "&uploaded=0"
        << "&downloaded=0"
        << "&left=" << torrent.getFileLength()
        << "&compact=1";

    // 2. Perform the HTTP GET request
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string responseBuffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    curl_easy_cleanup(curl);

    // 3. Decode the Bencoded response
    nlohmann::json decodedResponse;
    try
    {
        decodedResponse = Bencode::decode_bencoded_value(responseBuffer);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Failed to decode tracker response: " + std::string(e.what()));
    }

    // --- DEBUGGING CHANGE ---
    // Check for a "failure reason" key first. If it exists, the tracker
    // is telling us exactly what went wrong.
    if (decodedResponse.contains("failure reason"))
    {
        std::string reason = decodedResponse["failure reason"].get<std::string>();
        throw std::runtime_error("Tracker error: " + reason);
    }

    if (!decodedResponse.contains("peers"))
    {
        // If there's no failure reason and no peers, print the whole response.
        throw std::runtime_error("Tracker response missing 'peers' key. Full response: " + decodedResponse.dump());
    }

    // 4. Parse the compact peer list
    std::string peers_str = decodedResponse["peers"].get<std::string>();
    std::vector<std::string> peerList;
    for (size_t i = 0; i < peers_str.length(); i += 6)
    {
        std::string ip = std::to_string(static_cast<unsigned char>(peers_str[i])) + "." +
                         std::to_string(static_cast<unsigned char>(peers_str[i + 1])) + "." +
                         std::to_string(static_cast<unsigned char>(peers_str[i + 2])) + "." +
                         std::to_string(static_cast<unsigned char>(peers_str[i + 3]));

        uint16_t peerPort = (static_cast<unsigned char>(peers_str[i + 4]) << 8) |
                            (static_cast<unsigned char>(peers_str[i + 5]));

        peerList.push_back(ip + ":" + std::to_string(peerPort));
    }

    return peerList;
}
