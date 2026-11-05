#pragma once
#include <stdint.h>
#define FC3_FRAME_CHUNKS 4
#define FC3_FRAME_CHUNK_BYTES 24000
typedef struct { uint8_t *chunks[FC3_FRAME_CHUNKS]; } FC3Frame;
