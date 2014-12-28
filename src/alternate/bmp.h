#if !defined(HHXCB_BMP_H)
struct __attribute__((__packed__)) bmp_file_header {
    char bfType[2];
    uint32 bfSize;
    uint16 bfReserved1;
    uint16 bfReserved2;
    uint32 bfOffBits;
};

struct __attribute__((__packed__)) bmp_info_header {
    uint32 biSize;
    int32 biWidth;
    int32 biHeight;
    uint16 biPlanes;
    uint16 biBitCount;
    uint32 biCompression;
    uint32 biSizeImage;
    uint32 biXPelsPerMeter;
    uint32 biYPelsPerMeter;
    uint32 biClrUsed;
    uint32 biClrImportant;
};

#define HHXCB_BMP_H
#endif
