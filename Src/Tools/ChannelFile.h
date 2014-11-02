
#pragma once

class ChannelFile
{
public:
  bool_t add(uint64_t id, const byte_t* data, uint32_t size);
  bool_t remove(uint64_t id);
  bool_t get(uint64_t id, byte_t* data, uint32_t& size);
  bool_t getCompressedBlock(uint64_t id, byte_t* data, uint32_t& size);
};
