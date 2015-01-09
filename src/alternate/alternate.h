#if !defined(HHXCB_ALTERNATE_H)

struct tilemap_position
{
    uint32 chunk:24;
    uint8 tile;
};

struct tilemap_chunk
{
    uint32 *tiles;
};

struct world_position
{
    union {
        tilemap_position tltile_x;
        uint32 tile_x;
    };
    union {
        tilemap_position tltile_y;
        uint32 tile_y;
    };

    union {
        real32 tile_rel_x;
        real32 x;
    };
    union {
        real32 tile_rel_y;
        real32 y;
    };
};

struct tileset
{
    uint width;
    uint height;
    bmp_data bmp;
};

struct game_location
{
    uint32 tile_x;
    uint32 tile_y;

    real32 x;
    real32 y;
};

struct sprite_data
{
    bmp_data *bmp;
    int bmp_x;
    int bmp_y;
    int bmp_width;
    int bmp_height;
    int bmp_offset_x;
    int bmp_offset_y;
    int flags;
};

struct game_object
{
    sprite_data *sprite;
    world_position location;
};

struct tile_map
{
    uint chunk_dim;

    uint32 tile_side_in_pixels;
    real32 tile_side_in_meters;
    real32 meters_to_pixels;

    uint32 chunk_count_x;
    uint32 chunk_count_y;

    tileset tile_set;

    tilemap_chunk *chunks;
};

#define BITSET(name, type, number) uint64 name##_bitset; type name[number];
#define BITSET_ALLOCATE(object, name, type) (type *)bitset_allocate(object->name, &object->name##_bitset, sizeof(type))

#define BMP_ALLOCATE(object) BITSET_ALLOCATE(object, bmps, bmp_data)
#define GAME_OBJECT_ALLOCATE(object) BITSET_ALLOCATE(object, objects, game_object)
#define SPRITE_ALLOCATE(object) BITSET_ALLOCATE(object, sprites, sprite_data)

struct game_state
{
    tile_map tilemap;
    wav_data wav;
    bmp_data bmp;

    BITSET(bmps, bmp_data, 64)

    BITSET(objects, game_object, 64)

    BITSET(sprites, sprite_data, 64)

    game_object *player;
    game_object *player2;
};

#define HHXCB_ALTERNATE_H
#endif
