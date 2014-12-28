#if !defined(HHXCB_WAV_H)
#include "../handmade_platform.h"

struct __attribute__((__packed__)) wav_file_header {
    char ckID[4];
    uint32 cksize;
    char WAVEID[4];
};

struct __attribute__((__packed__)) wav_format_chunk {
    char ckID[4];
    uint32 cksize;
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSecond;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
};

struct __attribute__((__packed__)) wav_fact_chunk {
    char ckID[4];
    uint32 cksize;
    uint32 dwSampleLength;
};

struct __attribute__((__packed__)) wav_data_chunk {
    char ckID[4];
    uint32 cksize;
    uint8 data[];
};

struct wav_data {
    void *memory;
    int fd;

    int num_data_chunks;
    int current_data_chunk;
    int position_in_chunk;
    int sample_size;

    wav_format_chunk *format;
    wav_data_chunk *chunks[64];
};

void wav_open(char *filename, wav_data *data);
#define HHXCB_WAV_H
#endif
