/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "io/ZlibStream.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/* ZlibBaseStream */

ZlibBaseStream::ZlibBaseStream()
    : ZlibBaseStream(this) {
}

ZlibBaseStream::ZlibBaseStream(DataStream* other)
    : BaseStream(other)
    , outputBuffer_(16384U)
{
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;
}

bool ZlibBaseStream::isFinished() const {
  return finished_;
}

/* ZlibCompressStream */

ZlibCompressStream::ZlibCompressStream(bool gzip, int level)
  : ZlibCompressStream(this, gzip, level) {
}

ZlibCompressStream::ZlibCompressStream(DataStream* other, bool gzip, int level)
  : ZlibBaseStream(other)
{
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;

  if (deflateInit2(
      &strm_,
      level,
      Z_DEFLATED /* method */,
      15 + (gzip ? 16 : 0) /* windowBits */,
      8 /* memLevel */,
      Z_DEFAULT_STRATEGY /* strategy */) != Z_OK) {
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib deflateInit2 failed");
  }

  valid_ = true;
}

ZlibCompressStream::~ZlibCompressStream() {
  if (valid_) {
    deflateEnd(&strm_);
    valid_ = false;
  }
}

int ZlibCompressStream::writeData(uint8_t* value, int size) {
  if (!valid_) {
    return -1;
  }

  strm_.next_in = value;
  strm_.avail_in = size;

  do {
    strm_.next_out = outputBuffer_.data();
    strm_.avail_out = outputBuffer_.size();

    int ret = deflate(&strm_, value == nullptr ? Z_FINISH : Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR) {
      return -1;
    }
    int output_size = outputBuffer_.size() - strm_.avail_out;
    if (BaseStream::writeData(outputBuffer_.data(), output_size) != output_size) {
      return -1;
    }
  } while (strm_.avail_out == 0);

  return size;
}

void ZlibCompressStream::closeStream() {
  writeData(nullptr, 0U);

  finished_ = true; // TODO

  if (valid_) {
    deflateEnd(&strm_);
    valid_ = false;
  }
}

/* ZlibDecompressStream */

ZlibDecompressStream::ZlibDecompressStream(bool gzip)
  : ZlibDecompressStream(this, gzip) {
}

ZlibDecompressStream::ZlibDecompressStream(DataStream* other, bool gzip)
    : ZlibBaseStream(other)
{
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;

  if (inflateInit2(&strm_, 15 + (gzip ? 16 : 0) /* windowBits */) != Z_OK) {
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib inflateInit2 failed");
  }

  valid_ = true;
}

ZlibDecompressStream::~ZlibDecompressStream() {
  if (valid_) {
    inflateEnd(&strm_);
    valid_ = false;
  }
}

int ZlibDecompressStream::writeData(uint8_t* value, int size) {
  if (!valid_ || finished_) {
    return -1;
  }

  strm_.next_in = value;
  strm_.avail_in = size;

  int ret;
  do {
    strm_.next_out = outputBuffer_.data();
    strm_.avail_out = outputBuffer_.size();

    ret = inflate(&strm_, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR ||
        ret == Z_NEED_DICT ||
        ret == Z_DATA_ERROR ||
        ret == Z_MEM_ERROR) {
      return -1;
    }
    int output_size = outputBuffer_.size() - strm_.avail_out;
    if (BaseStream::writeData(outputBuffer_.data(), output_size) != output_size) {
      return -1;
    }
  } while (strm_.avail_out == 0);

  if (ret == Z_STREAM_END) {
    finished_ = true;
  }

  return size;
}

void ZlibDecompressStream::closeStream() {
  if (valid_) {
    inflateEnd(&strm_);
    valid_ = false;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
