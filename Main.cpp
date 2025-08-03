#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

// Include the headers for all our new modules
#include "bencode.h"
#include "torrent_file.h"
#include "tracker.h"
#include "peer_connection.h"

// --- Helper function to parse peer IP and Port ---
// This remains a useful utility for main.
std::pair<std::string, int> parsePeerInfo(const std::string &peerInfo)
{
    size_t colonPos = peerInfo.find(':');
    if (colonPos == std::string::npos)
    {
        throw std::runtime_error("Invalid peer address format: " + peerInfo);
    }
    std::string ip = peerInfo.substr(0, colonPos);
    int port = std::stoi(peerInfo.substr(colonPos + 1));
    return {ip, port};
}

// --- Main Application Logic ---
int main(int argc, char *argv[])
{
    // Flush output immediately for better logging
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2)
    {
        std::cerr << "Usage: ./your_client <command> [args...]" << std::endl;
        return 1;
    }

    std::string command = argv[1];

    try
    {
        if (command == "decode")
        {
            if (argc < 3)
                throw std::runtime_error("Usage: ./your_client decode <bencoded_string>");

            nlohmann::json decoded = Bencode::decode_bencoded_value(argv[2]);
            std::cout << decoded.dump() << std::endl;
        }
        else if (command == "info")
        {
            if (argc < 3)
                throw std::runtime_error("Usage: ./your_client info <torrent_file>");

            TorrentFile torrent;
            if (torrent.loadFromFile(argv[2]))
            {
                torrent.printInfo();
            }
            else
            {
                return 1; // Error message was already printed by loadFromFile
            }
        }
        else if (command == "peers")
        {
            if (argc < 3)
                throw std::runtime_error("Usage: ./your_client peers <torrent_file>");

            TorrentFile torrent;
            if (!torrent.loadFromFile(argv[2]))
                return 1;

            Tracker tracker;
            std::string peerId = "01234567890123456789"; // A real client would generate this
            uint16_t port = 6881;

            std::vector<std::string> peers = tracker.getPeers(torrent, peerId, port);
            for (const auto &p : peers)
            {
                std::cout << p << std::endl;
            }
        }
        else if (command == "download")
        {
            if (argc < 5 || std::string(argv[2]) != "-o")
            {
                throw std::runtime_error("Usage: ./your_client download -o <output_file> <torrent_file>");
            }
            std::string outputFile = argv[3];
            std::string torrentFilePath = argv[4];

            // 1. Load torrent file metadata
            TorrentFile torrent;
            if (!torrent.loadFromFile(torrentFilePath))
                return 1;

            // 2. Get peer list from tracker
            Tracker tracker;
            std::string peerId = "00112233445566778899";
            uint16_t port = 6881;
            std::vector<std::string> peers = tracker.getPeers(torrent, peerId, port);
            if (peers.empty())
                throw std::runtime_error("No peers found.");

            // 3. Connect to the first available peer
            // NOTE: A more robust client would try multiple peers if one fails.
            auto [peerIp, peerPort] = parsePeerInfo(peers[0]);
            PeerConnection peer(peerIp, peerPort, torrent, peerId);

            if (!peer.connectAndHandshake())
            {
                throw std::runtime_error("Failed to connect and handshake with peer " + peers[0]);
            }

            // 4. Download all pieces sequentially
            std::vector<uint8_t> fullFileData;
            fullFileData.reserve(torrent.getFileLength());

            for (size_t i = 0; i < torrent.getNumPieces(); ++i)
            {
                std::vector<uint8_t> pieceData = peer.downloadPiece(i);
                fullFileData.insert(fullFileData.end(), pieceData.begin(), pieceData.end());
            }

            peer.disconnect();

            // 5. Write the complete file to disk
            std::cout << "Download complete. Writing to file: " << outputFile << std::endl;
            std::ofstream outFileStream(outputFile, std::ios::binary);
            if (!outFileStream.write(reinterpret_cast<const char *>(fullFileData.data()), fullFileData.size()))
            {
                throw std::runtime_error("Failed to write to output file.");
            }
            std::cout << "File saved successfully." << std::endl;
        }
        else
        {
            throw std::runtime_error("Unknown command: " + command);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}