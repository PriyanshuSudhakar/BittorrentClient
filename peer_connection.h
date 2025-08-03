#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward-declare TorrentFile to avoid circular dependencies
class TorrentFile;

// Represents a connection to a single peer
class PeerConnection
{
public:
    // Constructor requires info about the peer, the torrent, and our client ID
    PeerConnection(std::string ip, int port, const TorrentFile &torrent, std::string ourPeerId);
    ~PeerConnection();

    // Establishes the TCP connection, performs the handshake, and prepares for downloading.
    // Returns true on success.
    bool connectAndHandshake();

    // Downloads a single, complete piece from the peer.
    // Throws an exception on error.
    std::vector<uint8_t> downloadPiece(size_t pieceIndex);

    // Closes the connection.
    void disconnect();

private:
    // --- Private helper methods ---
    bool performHandshake();
    void sendMessage(uint8_t messageId, const std::vector<uint8_t> &payload = {});
    std::vector<uint8_t> receiveMessage();
    void requestBlock(size_t pieceIndex, size_t blockOffset, size_t blockLength);
    bool verifyPiece(const std::vector<uint8_t> &pieceData, size_t pieceIndex);

    // --- Member variables ---
    std::string m_ip;
    int m_port;
    const TorrentFile &m_torrent;
    std::string m_ourPeerId;

    int m_sockfd = -1; // Socket file descriptor
    std::vector<bool> m_peerBitfield;
};