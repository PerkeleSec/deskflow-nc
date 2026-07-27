/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardChunk.h"

#include "base/Log.h"
#include "base/String.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include <cstring>
#include <limits>

namespace {

void clearCachedData(std::string &dataCached)
{
  dataCached.clear();
  dataCached.shrink_to_fit();
}

bool wouldExceed(size_t currentSize, size_t extraSize, size_t limit)
{
  return currentSize > limit || extraSize > limit - currentSize;
}

} // namespace

ClipboardChunk::ClipboardChunk(size_t size) : Chunk(size)
{
  m_dataSize = size - s_clipboardChunkMetaSize;
}

ClipboardChunk *ClipboardChunk::start(ClipboardID id, uint32_t sequence, const std::string &size)
{
  size_t sizeLength = size.size();
  auto *start = new ClipboardChunk(sizeLength + s_clipboardChunkMetaSize);
  char *chunk = start->m_chunk;

  chunk[0] = id;
  std::memcpy(&chunk[1], &sequence, 4);
  chunk[5] = ChunkType::DataStart;
  memcpy(&chunk[6], size.c_str(), sizeLength);
  chunk[sizeLength + s_clipboardChunkMetaSize - 1] = '\0';

  return start;
}

ClipboardChunk *ClipboardChunk::data(ClipboardID id, uint32_t sequence, const std::string &data)
{
  size_t dataSize = data.size();
  auto *chunk = new ClipboardChunk(dataSize + s_clipboardChunkMetaSize);
  char *chunkData = chunk->m_chunk;

  chunkData[0] = id;
  std::memcpy(&chunkData[1], &sequence, 4);
  chunkData[5] = ChunkType::DataChunk;
  memcpy(&chunkData[6], data.c_str(), dataSize);
  chunkData[dataSize + s_clipboardChunkMetaSize - 1] = '\0';

  return chunk;
}

ClipboardChunk *ClipboardChunk::end(ClipboardID id, uint32_t sequence)
{
  auto *end = new ClipboardChunk(s_clipboardChunkMetaSize);
  char *chunk = end->m_chunk;

  chunk[0] = id;
  std::memcpy(&chunk[1], &sequence, 4);
  chunk[5] = ChunkType::DataEnd;
  chunk[s_clipboardChunkMetaSize - 1] = '\0';
  return end;
}

TransferState ClipboardChunk::assemble(
    deskflow::IStream *stream, std::string &dataCached, ClipboardID &id, uint32_t &sequence,
    ClipboardChunkAssemblyState &state, size_t maxDataSize
)
{
  using enum TransferState;
  uint8_t mark;
  std::string data;
  auto reset = [&]() {
    state = {};
    clearCachedData(dataCached);
  };

  if (!ProtocolUtil::readf(stream, kMsgDClipboard + 4, &id, &sequence, &mark, &data)) {
    reset();
    return Error;
  }

  if (id >= kClipboardEnd) {
    LOG_ERR("clipboard chunk invalid id: %d", id);
    reset();
    return Error;
  }

#ifdef DESKFLOW_NO_CLIPBOARD
  // Clipboard sharing is compiled out. The message above is still read so the
  // connection stays in sync with a stock peer, but nothing is accumulated:
  // upstream only checks the total against s_expectedSize when DataEnd
  // arrives, so a peer that streams DataChunk messages and never terminates
  // them can grow dataCached without bound. Since this build discards the
  // payload anyway, the safe thing is to never buffer a byte of it.
  dataCached.clear();

  switch (mark) {
  case ChunkType::DataStart:
    LOG_DEBUG("ignoring clipboard transfer, clipboard sharing not built in");
    return Started;
  case ChunkType::DataChunk:
    return InProgress;
  case ChunkType::DataEnd:
    return (id >= kClipboardEnd) ? Error : Finished;
  default:
    break;
  }

  LOG_ERR("clipboard transmission failed: unknown error");
  return Error;
#else
  if (mark == ChunkType::DataStart) {
    bool ok = false;
    const auto expected = QString::fromStdString(data).toULongLong(&ok);
    if (!ok || expected > std::numeric_limits<size_t>::max()) {
      LOG_ERR("clipboard invalid size header: %s", data.c_str());
      reset();
      return Error;
    }

    clearCachedData(dataCached);
    state.expectedSize = static_cast<size_t>(expected);
    state.active = true;

    if (state.expectedSize > maxDataSize) {
      LOG_ERR("clipboard size exceeds limit, size: %zu, limit: %zu", state.expectedSize, maxDataSize);
      reset();
      return Error;
    }

    LOG_DEBUG("start receiving clipboard data, expected size=%zu", state.expectedSize);
    return Started;
  } else if (mark == ChunkType::DataChunk) {
    if (!state.active) {
      LOG_ERR("clipboard data chunk before start");
      reset();
      return Error;
    }

    if (wouldExceed(dataCached.size(), data.size(), state.expectedSize)) {
      LOG_ERR(
          "clipboard size exceeds declared, size: %zu, declared: %zu", dataCached.size() + data.size(),
          state.expectedSize
      );
      reset();
      return Error;
    }

    dataCached.append(data);
    return TransferState::InProgress;
  } else if (mark == ChunkType::DataEnd) {
    if (!state.active) {
      LOG_ERR("clipboard end chunk before start");
      reset();
      return Error;
    }

    state.active = false;

    if (state.expectedSize != dataCached.size()) {
      LOG_ERR("corrupted clipboard data, expected size=%zu actual size=%zu", state.expectedSize, dataCached.size());
      reset();
      return Error;
    }
    return Finished;
  }

  LOG_ERR("unknown clipboard chunk mark");
  reset();
  return Error;
#endif
}

void ClipboardChunk::send(deskflow::IStream *stream, void *data)
{
  const auto *clipboardData = static_cast<ClipboardChunk *>(data);

  LOG_VERBOSE("sending clipboard chunk");

  const char *chunk = clipboardData->m_chunk;
  ClipboardID id = chunk[0];
  uint32_t sequence;
  std::memcpy(&sequence, &chunk[1], 4);
  uint8_t mark = chunk[5];
  std::string dataChunk(&chunk[6], clipboardData->m_dataSize);

  switch (mark) {
  case ChunkType::DataStart:
    LOG_VERBOSE("sending clipboard chunk start: size=%s", dataChunk.c_str());
    break;

  case ChunkType::DataChunk:
    LOG_VERBOSE("sending clipboard chunk data: size=%i", dataChunk.size());
    break;

  case ChunkType::DataEnd:
    LOG_VERBOSE("sending clipboard finished");
    break;

  default:
    break;
  }

  ProtocolUtil::writef(stream, kMsgDClipboard, id, sequence, mark, &dataChunk);
}
