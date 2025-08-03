#pragma once

#include <string>
#include <vector>
#include <cstdint> // For uint16_t

// Forward-declare TorrentFile to avoid including the full header here
class TorrentFile;

class Tracker
{
public:
    // Announces to the tracker and requests a list of peers.
    std::vector<std::string> getPeers(const TorrentFile &torrent, const std::string &peerId, uint16_t port) const;
};