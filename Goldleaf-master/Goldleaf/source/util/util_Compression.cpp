#include <util/util_Compression.hpp>
#include <zlib.h>

namespace util {

    bool ZlibDecompress(void *dst, size_t dst_size, const void *src, size_t src_size, int wbits) {
        z_stream strm = {
            .next_in = (z_const Bytef*)src,
            .avail_in = (uInt)src_size,
            .next_out = (Bytef*)dst,
            .avail_out = (uInt)dst_size
        };
        if(inflateInit2(&strm, wbits) != Z_OK) {
            return false;
        }
        if(inflate(&strm, Z_FINISH) != Z_STREAM_END) {
            return false;
        }
        if(!inflateEnd(&strm) != Z_OK) {
            return false;
        }
        return true;
    }

}
