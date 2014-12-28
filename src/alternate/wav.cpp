#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wav.h"

void
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
