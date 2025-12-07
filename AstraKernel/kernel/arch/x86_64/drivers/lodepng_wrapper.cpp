/* Wrapper for lodepng C++ functions to provide C linkage */
#include "types.h"

/* Forward declare lodepng functions */
extern unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h,
                                  const unsigned char* in, size_t insize);
extern const char* lodepng_error_text(unsigned code);

/* Provide C linkage wrappers */
extern "C" {

unsigned lodepng_decode32_c(unsigned char** out, unsigned* w, unsigned* h,
                            const unsigned char* in, size_t insize) {
    return lodepng_decode32(out, w, h, in, insize);
}

const char* lodepng_error_text_c(unsigned code) {
    return lodepng_error_text(code);
}

}

