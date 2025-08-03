#include "peer_connection.h"
#include "torrent_file.h"
#include "lib/sha1.hpp" // For piece verification

#include <iostream>
#include <stdexcept>
#include <cstring>   // For memcpy/memset
#include <algorithm> // For std::min
#include <deque>
#include <iomanip> // For std::fixed, std::setprecision

// --- FIX: Define NOMINMAX before including Windows headers ---
// This prevents the Windows headers from defining min() and max() as macros,
// which would conflict with std::min and std::max.
#define NOMINMAX

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
#endif

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h> // For ssize_t on Linux/macOS
#endif

namespace
{
    // --- Protocol Constants ---
    const size_t PIECE_BLOCK_SIZE = 16384; // 16 KB
    const uint8_t MSG_CHOKE = 0;
    const uint8_t MSG_UNCHOKE = 1;
    const uint8_t MSG_INTERESTED = 2;
    const uint8_t MSG_BITFIELD = 5;
    const uint8_t MSG_REQUEST = 6;
    const uint8_t MSG_PIECE = 7;

    // A helper struct for the download queue
    struct BlockRequest
    {
        size_t pieceIndex;
        size_t offset;
        size_t length;
    };

    // Safely sends a buffer over a socket, handling partial sends.
    void sendAll(int sockfd, const char *data, size_t length)
    {
        size_t totalSent = 0;
        while (totalSent < length)
        {
            ssize_t sent = send(sockfd, data + totalSent, length - totalSent, 0);
            if (sent == -1)
            {
                throw std::runtime_error("Failed to send data to peer.");
            }
            totalSent += sent;
        }
    }

    // Safely receives data from a socket, handling partial receives.
    void receiveAll(int sockfd, char *buffer, size_t length)
    {
        size_t totalReceived = 0;
        while (totalReceived < length)
        {
            ssize_t received = recv(sockfd, buffer + totalReceived, length - totalReceived, 0);
            if (received <= 0)
            {
                throw std::runtime_error("Failed to receive data from peer (connection lost).");
            }
            totalReceived += received;
        }
    }
}

// --- Constructor / Destructor ---

PeerConnection::PeerConnection(std::string ip, int port, const TorrentFile &torrent, std::string ourPeerId)
    : m_ip(std::move(ip)), m_port(port), m_torrent(torrent), m_ourPeerId(std::move(ourPeerId)) {}

PeerConnection::~PeerConnection()
{
    disconnect();
}

void PeerConnection::disconnect()
{
    if (m_sockfd != -1)
    {
        closesocket(m_sockfd);
        m_sockfd = -1;
    }
}

// --- Public Methods ---

bool PeerConnection::connectAndHandshake()
{
    try
    {
        // 1. Create and connect socket
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            return false;
#endif
        m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_sockfd < 0)
            return false;

        sockaddr_in peerAddr{};
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(m_port);
        peerAddr.sin_addr.s_addr = inet_addr(m_ip.c_str());

        if (connect(m_sockfd, (struct sockaddr *)&peerAddr, sizeof(peerAddr)) == -1)
        {
            disconnect();
            return false;
        }

        // 2. Perform BitTorrent handshake
        if (!performHandshake())
        {
            disconnect();
            return false;
        }
        std::cout << "Handshake successful with " << m_ip << ":" << m_port << std::endl;

        // 3. Receive initial Bitfield message
        auto bitfieldMsg = receiveMessage();
        if (bitfieldMsg.empty() || bitfieldMsg[0] != MSG_BITFIELD)
        {
            throw std::runtime_error("Expected bitfield message after handshake.");
        }

        // 4. Send Interested and wait for Unchoke
        sendMessage(MSG_INTERESTED);
        auto unchokeMsg = receiveMessage();
        if (unchokeMsg.empty() || unchokeMsg[0] != MSG_UNCHOKE)
        {
            throw std::runtime_error("Peer did not send UNCHOKE.");
        }
        std::cout << "Peer unchoked us. Ready to download." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during connect/handshake: " << e.what() << std::endl;
        disconnect();
        return false;
    }
    return true;
}

std::vector<uint8_t> PeerConnection::downloadPiece(size_t pieceIndex)
{
    size_t pieceSize = m_torrent.getPieceLength();
    if (pieceIndex == m_torrent.getNumPieces() - 1)
    {
        pieceSize = m_torrent.getFileLength() % m_torrent.getPieceLength();
        if (pieceSize == 0)
            pieceSize = m_torrent.getPieceLength();
    }

    std::vector<uint8_t> pieceData(pieceSize);
    size_t downloaded = 0;

    std::deque<BlockRequest> requestQueue;

    for (size_t offset = 0; offset < pieceSize; offset += PIECE_BLOCK_SIZE)
    {
        size_t blockLength = std::min(PIECE_BLOCK_SIZE, pieceSize - offset);
        requestQueue.push_back({pieceIndex, offset, blockLength});
        requestBlock(pieceIndex, offset, blockLength);
    }

    while (downloaded < pieceSize)
    {
        auto msg = receiveMessage();
        if (msg.empty() || msg[0] != MSG_PIECE)
        {
            throw std::runtime_error("Unexpected message received while downloading piece.");
        }

        uint32_t receivedIndex_n, receivedBegin_n;
        std::memcpy(&receivedIndex_n, &msg[1], sizeof(uint32_t));
        std::memcpy(&receivedBegin_n, &msg[5], sizeof(uint32_t));

        size_t receivedIndex = ntohl(receivedIndex_n);
        size_t receivedBegin = ntohl(receivedBegin_n);
        size_t blockLength = msg.size() - 9;

        if (receivedIndex != pieceIndex)
        {
            throw std::runtime_error("Received piece index does not match requested index.");
        }

        std::memcpy(&pieceData[receivedBegin], &msg[9], blockLength);
        downloaded += blockLength;

        double progress = static_cast<double>(downloaded) / pieceSize * 100.0;
        std::cout << "\rDownloading piece " << pieceIndex << ": " << std::fixed << std::setprecision(2) << progress << "%" << std::flush;
    }
    std::cout << std::endl;

    if (!verifyPiece(pieceData, pieceIndex))
    {
        throw std::runtime_error("Piece verification failed!");
    }

    return pieceData;
}

// --- Private Helper Methods ---

bool PeerConnection::performHandshake()
{
    char handshakeMsg[68];
    handshakeMsg[0] = 19;
    std::memcpy(&handshakeMsg[1], "BitTorrent protocol", 19);
    std::memset(&handshakeMsg[20], 0, 8);
    std::memcpy(&handshakeMsg[28], m_torrent.getInfoHashBinary().c_str(), 20);
    std::memcpy(&handshakeMsg[48], m_ourPeerId.c_str(), 20);

    sendAll(m_sockfd, handshakeMsg, sizeof(handshakeMsg));
    char response[68];
    receiveAll(m_sockfd, response, sizeof(response));

    if (std::memcmp(&response[28], m_torrent.getInfoHashBinary().c_str(), 20) != 0)
    {
        return false;
    }
    return true;
}

void PeerConnection::sendMessage(uint8_t messageId, const std::vector<uint8_t> &payload)
{
    uint32_t len = htonl(1 + payload.size());
    sendAll(m_sockfd, reinterpret_cast<const char *>(&len), 4);
    sendAll(m_sockfd, reinterpret_cast<const char *>(&messageId), 1);
    if (!payload.empty())
    {
        sendAll(m_sockfd, reinterpret_cast<const char *>(payload.data()), payload.size());
    }
}

std::vector<uint8_t> PeerConnection::receiveMessage()
{
    uint32_t len_n;
    receiveAll(m_sockfd, reinterpret_cast<char *>(&len_n), 4);
    uint32_t len = ntohl(len_n);

    if (len == 0)
    {
        return {};
    }

    std::vector<uint8_t> msg(len);
    receiveAll(m_sockfd, reinterpret_cast<char *>(msg.data()), len);
    return msg;
}

void PeerConnection::requestBlock(size_t pieceIndex, size_t blockOffset, size_t blockLength)
{
    std::vector<uint8_t> payload(12);
    uint32_t index_n = htonl(pieceIndex);
    uint32_t begin_n = htonl(blockOffset);
    uint32_t length_n = htonl(blockLength);

    std::memcpy(&payload[0], &index_n, 4);
    std::memcpy(&payload[4], &begin_n, 4);
    std::memcpy(&payload[8], &length_n, 4);

    sendMessage(MSG_REQUEST, payload);
}

bool PeerConnection::verifyPiece(const std::vector<uint8_t> &pieceData, size_t pieceIndex)
{
    SHA1 sha1;
    sha1.update(std::string(pieceData.begin(), pieceData.end()));
    std::string calculatedHash = sha1.final();

    std::string expectedHash = m_torrent.getPieceHashes().substr(pieceIndex * 20, 20);

    return calculatedHash == expectedHash;
}
