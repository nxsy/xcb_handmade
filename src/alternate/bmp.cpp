#include "../handmade_platform.h"
#include "../alternate/bmp.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>

int bmp_open(char *filename, bmp_data *data)
{
    data->fd = open(filename, O_RDONLY);
    if (data->fd < 0)
    {
        printf("Failed to open file: %s\n", filename);
        return -1;
    }
    struct stat statbuf = {};
    uint32_t stat_result = fstat(data->fd, &statbuf);
    if (stat_result != 0)
    {
        printf("Failed to stat opened file: %s (fd %i)\n", filename, data->fd);
        return -1;
    }
    data->memory = mmap((void *)0x10000000000, statbuf.st_size, PROT_READ, MAP_PRIVATE, data->fd, 0);

    bmp_file_header *h = (bmp_file_header *)data->memory;
    if (h->bfType[0] != 'B' || h->bfType[1] != 'M')
    {
        printf("Incorrect file format: %.2s, not BM", h->bfType);
        return -1;
    }

    data->bmp_memory = (void *)((uint8 *)data->memory + h->bfOffBits);

    bmp_info_header *info = (bmp_info_header *)((uint8 *)data->memory + sizeof(bmp_file_header));
    if (info->biSize != 40)
    {
        printf("Unsupported biSize: %i, not 40", info->biSize);
        return -1;
    }
    if (info->biCompression) {
        printf("Unsupported Compression: %i, not 0", info->biCompression);
        return -1;
    }

    data->width = info->biWidth;
    data->height = info->biHeight;
    data->top_to_bottom = info->biHeight < 0;
    data->bits_per_pixel = info->biBitCount;
    return 0;
}

