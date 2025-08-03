#include "torrent_file.h"

// Include necessary implementation headers
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "bencode.h" // Your bencode module
#include "lib/sha1.hpp"

// Helper function to convert binary bytes to a hex string
// This is used for both the info hash and the piece hashes
namespace
{
    std::string bytesToHex(const std::string &bytes)
    {
        std::ostringstream hexStream;
        hexStream << std::hex << std::setfill('0');
        for (unsigned char byte : bytes)
        {
            hexStream << std::setw(2) << static_cast<int>(byte);
        }
        return hexStream.str();
    }
}

bool TorrentFile::loadFromFile(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Failed to open file: " << filepath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();

    try
    {
        nlohmann::json decodedTorrent = Bencode::decode_bencoded_value(fileContent);

        // Bencode the 'info' dictionary to calculate the info hash
        std::string bencodedInfo = Bencode::json_to_bencode(decodedTorrent["info"]);

        // Calculate info hash (binary and hex)
        SHA1 sha1;
        sha1.update(bencodedInfo);
        m_infoHashBinary = sha1.final();
        m_infoHashHex = bytesToHex(m_infoHashBinary);

        // Extract metadata
        m_trackerUrl = decodedTorrent["announce"].get<std::string>();
        m_fileLength = decodedTorrent["info"]["length"].get<size_t>();
        m_pieceLength = decodedTorrent["info"]["piece length"].get<size_t>();
        m_fileName = decodedTorrent["info"]["name"].get<std::string>();
        m_pieceHashes = decodedTorrent["info"]["pieces"].get<std::string>();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: Failed to parse torrent file. " << e.what() << std::endl;
        return false;
    }

    return true;
}

void TorrentFile::printInfo() const
{
    std::cout << "Tracker URL: " << m_trackerUrl << std::endl;
    std::cout << "File Name:   " << m_fileName << std::endl;
    std::cout << "File Length: " << m_fileLength << " bytes" << std::endl;
    std::cout << "Piece Length:" << m_pieceLength << " bytes" << std::endl;
    std::cout << "Num Pieces:  " << getNumPieces() << std::endl;
    std::cout << "Info Hash:   " << m_infoHashHex << std::endl;
    std::cout << "Piece Hashes: " << std::endl;

    for (size_t i = 0; i < m_pieceHashes.length(); i += 20)
    {
        std::cout << "  " << bytesToHex(m_pieceHashes.substr(i, 20)) << std::endl;
    }
}

// --- Implementation of Getters ---

const std::string &TorrentFile::getTrackerUrl() const { return m_trackerUrl; }
const std::string &TorrentFile::getInfoHashHex() const { return m_infoHashHex; }
const std::string &TorrentFile::getInfoHashBinary() const { return m_infoHashBinary; }
const std::string &TorrentFile::getPieceHashes() const { return m_pieceHashes; }
const std::string &TorrentFile::getFileName() const { return m_fileName; }
size_t TorrentFile::getPieceLength() const { return m_pieceLength; }
size_t TorrentFile::getFileLength() const { return m_fileLength; }
size_t TorrentFile::getNumPieces() const
{
    if (m_pieceLength == 0)
        return 0;
    return (m_fileLength + m_pieceLength - 1) / m_pieceLength;
}