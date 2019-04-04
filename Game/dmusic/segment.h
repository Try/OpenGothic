#pragma once

#include <Tempest/IDevice>
#include <vector>
#include <cstring>
#include <memory>

namespace Dx8 {

class RiffFile;

class Segment {
  public:
    Segment(Tempest::IDevice& dev);

  private:
    struct ChunkHeader final {
      char          id[4]={};
      std::uint32_t size =0;
      bool is(const char* idx) const { return std::memcmp(id,idx,4)==0; }
      };

    struct Chunk final {
      char               id[4]={};
      char               listId[4]={};
      std::vector<Chunk> nested;

      bool listIs(const char* idx) const { return std::memcmp(listId,idx,4)==0; }
      };

    struct GUID {
      std::uint32_t Data1 = 0;
      std::uint16_t Data2 = 0;
      std::uint16_t Data3 = 0;
      std::uint64_t Data4 = 0;
      };

    struct DMUS_IO_SEGMENT_HEADER {
      std::uint32_t dwRepeats    = 0;
      std::uint32_t mtLength     = 0;
      std::uint32_t mtPlayStart  = 0;
      std::uint32_t mtLoopStart  = 0;
      std::uint32_t mtLoopEnd    = 0;
      std::uint32_t dwResolution = 0;
      std::uint64_t rtLength     = 0;
      std::uint32_t dwFlags      = 0;
      std::uint32_t dwReserved   = 0;
      std::uint64_t rtLoopStart  = 0;
      std::uint32_t rtLoopEnd    = 0;
      };

    struct DMUS_IO_TRACK_HEADER {
      GUID          guidClassID;
      std::uint32_t dwPosition=0;
      std::uint32_t dwGroup   =0;

      // Identifier of the track's data chunk.
      // If this value is 0, it is assumed that the chunk is of type LIST,
      // so fccType is valid and must be nonzero.
      char ckid[4]={};

      // List type. If this value is 0, ckid is valid and must be nonzero.
      char fccType[4]={};
      };

    struct DMUS_IO_VERSION {
      std::uint32_t dwVersionHi = 0;
      std::uint32_t dwVersionLo = 0;
      };

    struct DMUS_IO_REFERENCE {
      GUID          guidClassID;
      std::uint32_t dwValidData=0;
      };

    class ReferenceList {
      public:
        ReferenceList(RiffFile& dev);

        DMUS_IO_REFERENCE header;
        GUID              guid;
        std::u16string    name, file, category;
        DMUS_IO_VERSION   version;
      };

    using StyleReference = std::pair<std::uint16_t, ReferenceList>;

    struct TrackData {
      virtual ~TrackData()=default;
      };

    struct Track {
      GUID                          guid;
      DMUS_IO_VERSION               version;
      DMUS_IO_TRACK_HEADER          header;
      //DirectMusic::Riff::Unfo m_unfo;
      //DMUS_IO_TRACK_EXTRAS_HEADER   flags;
      std::unique_ptr<TrackData>    data;
      };

    struct TrackCoord : TrackData {
      TrackCoord(RiffFile& dev);

      std::uint32_t          header=0;
      //std::vector<ChordBody> chords;
      };

    struct TrackStyle: TrackData {
      TrackStyle(RiffFile& dev);
      std::vector<StyleReference> styles;

      void readStyle(RiffFile& dev);
      };

    void readChunk (RiffFile& dev, Chunk& ch);
    void readNested(RiffFile& dev, Chunk& ch);
    void readTracks(RiffFile& dev, Chunk& ch);
    void readTrack (RiffFile& dev, Track& tr);

    Chunk                  root;
    GUID                   guid;
    DMUS_IO_SEGMENT_HEADER header;
    DMUS_IO_VERSION        version;

    std::vector<Track>     tracks;
  };

}
