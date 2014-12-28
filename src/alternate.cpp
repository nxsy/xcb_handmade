#include "handmade_platform.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void
wav_open(char *filename, wav_data *data) {
    data->fd = open(filename, O_RDONLY);
    struct stat statbuf = {};
    uint32_t stat_result = fstat(data->fd, &statbuf);
    data->memory = mmap(0, statbuf.st_size, PROT_READ, MAP_PRIVATE, data->fd, 0);

    wav_file_header *h = (wav_file_header *)data->memory;
    int offset = 12;
    wav_format_chunk *f = (wav_format_chunk *)((uint8 *)data->memory + offset);
    data->format = f;

    offset += 8 + f->cksize;
    wav_fact_chunk *fact = (wav_fact_chunk *)((uint8 *)data->memory + offset);
    if (fact->ckID[0] == 'f' && fact->ckID[1] == 'a' && fact->ckID[2] == 'c' && fact->ckID[3] == 't')
    {
        offset += 8 + fact->cksize;
    }

    wav_data_chunk *data_chunk = (wav_data_chunk *)((uint8 *)data->memory + offset);

    data->chunks[0] = data_chunk;
    data->num_data_chunks = 1;
    data->sample_size = f->nChannels * (f->wBitsPerSample / 8);
}

struct game_state
{
    wav_data wav;
};

// TODO(nbm): Wait for platform layer to support something like this
void
find_data_file(char *seek_filename, uint16 filename_max_length, char *full_filename)
{
    char *paths[3] = {
        (char *)"data",
        (char *)"../data",
        (char *)"../../data",
    };
    struct stat statbuf = {};
    for (int i = 0;
            i < 3;
            ++i)
    {
        char *path = paths[i];
        snprintf(full_filename, filename_max_length, "%s/%s", path, seek_filename);
        uint32_t stat_result = stat(full_filename, &statbuf);
        if (stat_result == 0)
        {
            return;
        }
    }
    full_filename[0] = 0;
    return;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state *state = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        state->wav = {};
        char filename[1024];
        find_data_file((char *)"Loop-Menu.wav", sizeof(filename), filename);
        wav_open(filename, &state->wav);

        Memory->IsInitialized = true;
    }

    uint8 *row = (uint8 *)Buffer->Memory;
    for (uint rowcount = 0;
            rowcount < Buffer->Height;
            rowcount++)
    {
        uint8 *next_row = row + Buffer->Pitch;
        for (uint32 *pixel = (uint32 *)row;
                pixel < (uint32 *)next_row;
                ++pixel)
        {
            *pixel = (rand() % 256) << 16 |
                (rand() % 256) << 8 |
                (rand() % 256) << 0;
        }

        row = next_row;
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    // TODO(nbm): Rather make this an assert and change the platform layer to
    // avoid this situation.
    if (SoundBuffer->SampleCount <= 0)
    {
        return;
    }

    game_state *state = (game_state *)Memory->PermanentStorage;
    wav_data_chunk *data_chunk = state->wav.chunks[state->wav.current_data_chunk];
    uint8 *data = data_chunk->data;
    int samples_in_chunk = data_chunk->cksize / state->wav.sample_size;
    uint32 samples_needed = SoundBuffer->SampleCount;

    uint32 *sample_output = (uint32 *)SoundBuffer->Samples;

    while (samples_needed) {
        uint8 *start_data = data + (state->wav.position_in_chunk * state->wav.sample_size);
        int32 samples_left_in_chunk = samples_in_chunk - state->wav.position_in_chunk;
        int32 samples_this_time = samples_needed;
        bool32 go_to_next_chunk = 0;
        if (samples_needed > samples_left_in_chunk) {
            samples_this_time = samples_left_in_chunk;
        }
        uint32 bytes_to_copy = samples_this_time * state->wav.sample_size;
        memcpy(sample_output, start_data, bytes_to_copy);

        state->wav.position_in_chunk += samples_this_time;
        samples_needed -= samples_this_time;
        sample_output += samples_this_time;

        // Need to go to next data chunk, or loop back to the beginning of the
        // first chunk if at the end.
        // TODO(nbm): Should also/rather check position_in_chunk >
        // samples_in_chunk?
        if (samples_needed)
        {
            state->wav.current_data_chunk++;
            state->wav.current_data_chunk %= state->wav.num_data_chunks;
            state->wav.position_in_chunk = 0;
        }
    }
}
