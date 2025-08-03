#pragma once

#include <string>
#include <vector>
#include <cstddef> // For size_t

class TorrentFile
{
public:
    // Tries to load and parse a .torrent file. Returns true on success.
    bool loadFromFile(const std::string &filepath);

    // Prints all the parsed information to the console for debugging.
    void printInfo() const;

    // --- Getters for Torrent Metadata ---
    const std::string &getTrackerUrl() const;
    const std::string &getInfoHashHex() const;
    const std::string &getInfoHashBinary() const;
    const std::string &getPieceHashes() const;
    const std::string &getFileName() const;
    size_t getPieceLength() const;
    size_t getFileLength() const;
    size_t getNumPieces() const;

private:
    std::string m_trackerUrl;
    std::string m_infoHashHex;
    std::string m_infoHashBinary;
    std::string m_pieceHashes;
    std::string m_fileName;
    size_t m_pieceLength = 0;
    size_t m_fileLength = 0;
};