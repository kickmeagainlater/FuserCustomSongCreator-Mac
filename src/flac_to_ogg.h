#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Decode a FLAC file from disk and re-encode it to Ogg Vorbis (in-memory).
// Returns true on success. On failure, `err` is populated.
bool convertFlacToOggVorbis(const std::string& flacPath,
                            std::vector<uint8_t>& outOggData,
                            std::string& err);
