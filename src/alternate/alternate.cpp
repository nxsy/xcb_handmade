#include "../handmade_platform.h"

#include <sys/stat.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../alternate/wav.h"
#include "../alternate/wav.cpp"
#include "../alternate/bmp.h"
#include "../alternate/bmp.cpp"

#include "../alternate/alternate.h"

struct game_object_list {
    game_object *object;
    game_object_list *next;
};

bool32 alternate_debug = 0;

static void *
bitset_allocate(void *data, uint64 *bitset, uint32 size)
{
    uint8 *databytes = (uint8 *)data;
    int found = -1;
    for (int i = 0;
            i < 64;
            ++i)
    {
        if ((*bitset & (uint64)pow(2, i)) == 0)
        {
            found = i;
            break;
        }
    }
    if (found < 0)
    {
        return 0;
    }
    *bitset |= (uint64)pow(2, found);
    return (void *)(databytes + (found * size));
}

static void
iterate_bitset_array(void *data, uint64 bitset, uint32 size, void **next)
{
    uint8 *databytes = (uint8 *)data;
    int8 offset = -1;

    if (*next)
    {
        uint8 *nextbytes = (uint8 *)*next;
        offset = (nextbytes - databytes) / size;
    }

    int8 next_offset = -1;
    for (int i = offset + 1;
            i < 64;
            ++i)
    {
        if ((bitset & (uint64)pow(2, i)) != 0)
        {
            next_offset = i;
            break;
        }

    }
    if (next_offset < 0) {
        *next = 0;
    }
    else
    {
        *next = (void *)(databytes + (next_offset * size));
    }
}

/*
static void
bitset_free(void *base, uint64 *bitset, uint32 size, void *to_free)
{
    uint8 *base_bytes = (uint8 *)base;
    uint8 *to_free_bytes = (uint8 *)to_free;

    uint offset = (to_free_bytes - base_bytes) / size;
    *bitset &= ~((uint64)pow(2, offset));
}
*/

#define DRAW_BMP_INVERTED 1
#define DRAW_BMP_HIGHLIGHTED 2

// TODO(nbm): Wait for platform layer to support something like this
static void
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

static void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int buffer_x, int buffer_y, int bmp_x, int bmp_y, int bmp_width, int bmp_height, int flags)
{
    bool32 draw_inverted = flags & DRAW_BMP_INVERTED;
    int8 r_offset = 0;
    int8 g_offset = 0;
    int8 b_offset = 0;
    if (flags & DRAW_BMP_HIGHLIGHTED)
    {
        r_offset = -10;
        g_offset = -10;
        b_offset = -10;
    }
    int buffer_max_x = buffer_x + bmp_width;
    int buffer_max_y = buffer_y + bmp_height;

    if (buffer_max_y > buffer->Height)
    {
        buffer_max_y = buffer->Height;
    }

    uint16 bmp_pitch = data->width * (data->bits_per_pixel / 8);
    uint16 row_length = bmp_width * (data->bits_per_pixel / 8);

    uint8 *bmp_first_row = (uint8 *)data->bmp_memory;
    uint8 *bmp_final_row = bmp_first_row + (bmp_pitch * (data->height - 1));

    uint8 *bmp_row;
    int8 bmp_direction;

    if (data->top_to_bottom)
    {
        bmp_direction = 1;
        bmp_row = bmp_first_row;
    }
    else
    {
        bmp_direction = -1;
        bmp_row = bmp_final_row;
    }

    bmp_row += (bmp_pitch * bmp_direction * bmp_y);

    if (draw_inverted == 0)
    {
        bmp_row += (bmp_pitch * bmp_direction * (bmp_height - 1));
        bmp_direction = -bmp_direction;
    }

    uint8 *buffer_row = (uint8 *)buffer->Memory + ((buffer->Height - 1 - buffer_y) * buffer->Pitch);
    while (buffer_y < buffer_max_y)
    {
        uint8 *next_buffer_row = buffer_row - buffer->Pitch;
        uint8 *last_buffer_pixel = buffer_row + buffer->Pitch;

        uint8 *bmp_next_row = bmp_row + (bmp_pitch * bmp_direction);
        uint32 *buffer_pixel = (uint32 *)buffer_row;
        uint32 *bmp_row_pixel = (uint32 *)bmp_row + bmp_x;
        uint32 *bmp_final_row_pixel = bmp_row_pixel + bmp_width;

        if (buffer_x > 0)
        {
            buffer_pixel += buffer_x;
        }
        else if (buffer_x < 0)
        {
            bmp_row_pixel -= buffer_x;
        }

        if (buffer_y >= 0) {
            while(bmp_row_pixel < bmp_final_row_pixel && buffer_pixel < (uint32 *)last_buffer_pixel)
            {
                uint8 *subpixel = (uint8 *)bmp_row_pixel;
                uint8 *alpha = subpixel + 3;
                if (*alpha == 255) {
                    *buffer_pixel = *bmp_row_pixel;
                }
                if (r_offset || g_offset || b_offset)
                {
                    uint8 *b = (uint8 *)buffer_pixel;
                    uint8 *g = b + 1;
                    uint8 *r = g + 1;
                    *b = *b + b_offset;
                    *r = *r + r_offset;
                    *g = *g + g_offset;
                }
                buffer_pixel++;
                bmp_row_pixel++;
            }
        }

        buffer_row = next_buffer_row;
        bmp_row = bmp_next_row;;
        ++buffer_y;
    }
}

static void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int buffer_x, int buffer_y, int bmp_x, int bmp_y, int bmp_width, int bmp_height)
{
    draw_bmp(buffer, data, buffer_x, buffer_y, bmp_x, bmp_y, bmp_width, bmp_height, 0);
}

static void
draw_tile(game_offscreen_buffer *buffer, tileset *t, int tilenumber, int x, int y, int flags)
{
    if (tilenumber == -1)
    {
        return;
    }
    int bmp_x = (t->width * tilenumber) % t->bmp.width;
    int bmp_y = t->height * ((t->width * tilenumber) / t->bmp.width);
    draw_bmp(buffer, &t->bmp, x, y, bmp_x, bmp_y, t->width, t->height, flags);
}

inline tilemap_chunk *
get_tile_chunk(tile_map *tilemap, uint32 x, uint32 y)
{
    tilemap_chunk *chunk = 0;

    if((x >= 0) && (x < tilemap->chunk_count_x) &&
       (y >= 0) && (y < tilemap->chunk_count_y))
    {
        chunk = &tilemap->chunks[
            y * tilemap->chunk_count_x +
            x];
    }

    return chunk;
}

inline uint32
get_tile_value(tile_map *tilemap, tilemap_chunk *chunk, uint32 x, uint32 y)
{
    uint32 tile_value = 29;

    if(chunk && chunk->tiles)
    {
        tile_value = chunk->tiles[y*tilemap->chunk_dim + x];
    }

    return tile_value;
}

inline uint32
get_tile_value(tile_map *tilemap, uint32 abs_tile_x, uint32 abs_tile_y)
{
    uint32 tile_value = 200;
    tilemap_chunk *chunk = get_tile_chunk(tilemap, abs_tile_x >> 4, abs_tile_y >> 4);
    if(chunk && chunk->tiles)
    {
        tile_value = get_tile_value(tilemap, chunk, abs_tile_x & 0xF, abs_tile_y & 0xF);
    }

    return tile_value;
}

static void
set_tile_value(tile_map *tilemap,
        tilemap_chunk *chunk,
        uint32 x,
        uint32 y,
        uint32 tile_value)
{
    if(chunk && chunk->tiles)
    {
        chunk->tiles[y*tilemap->chunk_dim + x] = tile_value;
    }
}

static void
set_tile_value(tile_map *tilemap,
        uint32 abs_tile_x,
        uint32 abs_tile_y,
        uint32 tile_value)
{
    tilemap_chunk *chunk = get_tile_chunk(tilemap, abs_tile_x >> 4, abs_tile_y >> 4);
    if(!chunk->tiles)
    {
        uint32 tile_count = tilemap->chunk_dim * tilemap->chunk_dim;

        chunk->tiles = (uint32 *)calloc(sizeof(uint32), tile_count);
        for(uint32 i = 0;
            i < tile_count;
            ++i)
        {
            chunk->tiles[i] = 29;
        }
    }

    set_tile_value(tilemap, chunk, abs_tile_x & 0xF, abs_tile_y & 0xF, tile_value);
}

static void
populate_tilemap(game_state *state, tile_map *tilemap)
{
    uint32 tiles_per_width = 11;
    uint32 tiles_per_height = 7;
    uint32 screen_x = 0;
    uint32 screen_y = 0;

    bool32 door_left = false;
    bool32 door_right = false;
    bool32 door_top = false;
    bool32 door_bottom = false;

    for(uint32 screen_index = 0;
        screen_index < 16;
        ++screen_index)
    {
        uint32 random_choice = rand() % 2;

        if(random_choice == 1)
        {
            door_right = true;
        }
        else
        {
            door_top = true;
        }

        for(uint32 y = 0;
            y < tiles_per_height;
            ++y)
        {
            for(uint32 x = 0;
                x < tiles_per_width;
                ++x)
            {
                uint32 abs_tile_x = screen_x * tiles_per_width + x;
                uint32 abs_tile_y = screen_y * tiles_per_height + y;

                uint32 tile_value = 39;

                if(y == 0)
                {
                    if (x == 0)
                    {
                        tile_value = 13;
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        tile_value = 14;
                    }
                    else if (x == (tiles_per_width / 2))
                    {
                        if (!door_bottom)
                        {
                            tile_value = 45;
                        }
                    }
                    else if (x == (tiles_per_width / 2) - 1)
                    {
                        if (!door_bottom)
                        {
                            tile_value = 45;
                        }
                        else
                        {
                            tile_value = 46;
                        }
                    }
                    else if (x == (tiles_per_width / 2) + 1)
                    {
                        if (!door_bottom)
                        {
                            tile_value = 45;
                        }
                        else
                        {
                            tile_value = 44;
                        }
                    }
                    else {
                        tile_value = 45;
                    }
                }
                else if (y == (tiles_per_height - 1))
                {
                    if (x == 0)
                    {
                        tile_value = 31;
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        tile_value = 32;
                    }
                    else if (x == (tiles_per_width / 2))
                    {
                        if (!door_top)
                        {
                            tile_value = 45 | (1<<31);
                        }
                    }
                    else if (x == (tiles_per_width / 2) - 1)
                    {
                        if (!door_top)
                        {
                            tile_value = 45 | (1<<31);
                        }
                        else
                        {
                            tile_value = 46 | (1<<31);
                        }
                    }
                    else if (x == (tiles_per_width / 2) + 1)
                    {
                        if (!door_top)
                        {
                            tile_value = 45 | (1<<31);
                        }
                        else
                        {
                            tile_value = 44 | (1<<31);
                        }
                    }
                    else {
                        tile_value = 45 | (1<<31);
                    }
                }
                else if (y == (tiles_per_height / 2))
                {
                    if (x == 0)
                    {
                        if (!door_left) {
                            tile_value = 30;
                        }
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        if (!door_right)
                        {
                            tile_value = 28;
                        }
                    }
                }
                else if (y == (tiles_per_height / 2) - 1)
                {
                    if (x == 0)
                    {
                        if (!door_left) {
                            tile_value = 30;
                        }
                        else
                        {
                            tile_value = 46;
                        }
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        if (!door_right)
                        {
                            tile_value = 28;
                        }
                        else
                        {
                            tile_value = 44;
                        }
                    }
                }
                else if (y == (tiles_per_height / 2) + 1)
                {
                    if (x == 0)
                    {
                        if (!door_left) {
                            tile_value = 30;
                        }
                        else
                        {
                            tile_value = 46 | (1<<31);
                        }
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        if (!door_right)
                        {
                            tile_value = 28;
                        }
                        else
                        {
                            tile_value = 44 | (1<<31);
                        }
                    }
                }
                else
                {
                    if (x == 0)
                    {
                        tile_value = 30;
                    }
                    else if (x == (tiles_per_width - 1))
                    {
                        tile_value = 28;
                    }
                }

                set_tile_value(
                        tilemap,
                        abs_tile_x,
                        abs_tile_y,
                        tile_value);
            }
        }

        door_left = door_right;
        door_bottom = door_top;

        door_right = false;
        door_top = false;

        if(random_choice == 1)
        {
            screen_x += 1;
        }
        else
        {
            screen_y += 1;
        }
    }
}

void
draw_screen(game_offscreen_buffer *Buffer, game_state *state, game_object_list *head)
{
    real32 screen_center_x = 0.5f * (real32)Buffer->Width;
    real32 screen_center_y = 0.5f * (real32)Buffer->Height;

    uint32 mid_tile_y = screen_center_y / state->tilemap.tile_side_in_pixels;
    uint32 mid_tile_x = screen_center_x / state->tilemap.tile_side_in_pixels;

    real32 y_offset = 0;
    real32 x_offset = 0;

    uint32 bottom_row = 0;
    if ((state->player->location.tile_y > mid_tile_y) ||
            ((state->player->location.tile_y == mid_tile_y) && state->player->location.y > 0))
    {
        bottom_row = state->player->location.tile_y - mid_tile_y;
        y_offset = -(state->player->location.y * state->tilemap.meters_to_pixels);
    }
    uint32 top_row = bottom_row + ceil((real32)Buffer->Height / state->tilemap.tile_side_in_pixels);

    uint32 left_column = 0;
    if ((state->player->location.tile_x > mid_tile_x) ||
            ((state->player->location.tile_x == mid_tile_x) && state->player->location.x > 0))
    {
        left_column = state->player->location.tile_x - mid_tile_x;
        x_offset = -(state->player->location.x * state->tilemap.meters_to_pixels);
    }
    uint32 right_column = left_column + ceil((real32)Buffer->Width / state->tilemap.tile_side_in_pixels);

    for(uint32 row = bottom_row > 0 ? bottom_row - 1 : bottom_row;
        row < top_row + 1;
        ++row)
    {
        for(uint32 column = left_column > 0 ? left_column - 1 : left_column;
            column < right_column + 1;
            ++column)
        {
            uint32 tile_value = get_tile_value(&state->tilemap, column, row);

            int flags = DRAW_BMP_INVERTED;
            if (tile_value & (1 << 31))
            {
                flags &= ~DRAW_BMP_INVERTED;
                tile_value &= ~(1<<31);

            }
            uint32 x = (column - left_column) * state->tilemap.tile_side_in_pixels;
            uint32 y = (row - bottom_row) * state->tilemap.tile_side_in_pixels - 18;
            x += (int32)x_offset;
            y += (int32)y_offset;
            if (state->player->location.tile_x == column && state->player->location.tile_y == row)
            {
                flags |= DRAW_BMP_HIGHLIGHTED;
            }
            draw_tile(Buffer, &state->tilemap.tile_set, tile_value, x, y, flags);
        }
    }

    while (head)
    {
        game_object *gt = head->object;

        int32 object_tile_row = (int32)gt->location.tile_y - bottom_row;
        int32 object_tile_column = (int32)gt->location.tile_x - left_column;

        real32 object_pixel_row = (object_tile_row * (int32)state->tilemap.tile_side_in_pixels) +
            (gt->location.y * state->tilemap.meters_to_pixels);
        real32 object_pixel_column = (object_tile_column * (int32)state->tilemap.tile_side_in_pixels) +
            (gt->location.x * state->tilemap.meters_to_pixels);

        object_pixel_row += y_offset;
        object_pixel_column += x_offset;

        draw_bmp(Buffer, gt->sprite->bmp, object_pixel_column + gt->sprite->bmp_offset_x, object_pixel_row + gt->sprite->bmp_offset_y, gt->sprite->bmp_x, gt->sprite->bmp_y,  gt->sprite->bmp_width, gt->sprite->bmp_height);
        head = head->next;
    }

}

void
recanonicalize(tile_map *tilemap, uint32 *tile, real32 *rel)
{
    int32 offset = roundf(*rel / tilemap->tile_side_in_meters);
    *tile += offset;
    *rel -= offset * tilemap->tile_side_in_meters;
}

void
recanonicalize(tile_map *tilemap, world_position *location)
{
    recanonicalize(tilemap, &location->tile_x, &location->x);
    recanonicalize(tilemap, &location->tile_y, &location->y);
}

inline bool32
location_greater(world_position i, world_position j)
{
    bool32 ret = false;
    if (i.tile_y > j.tile_y)
    {
        ret = true;
    }
    else if (i.tile_y == j.tile_y)
    {
        if (i.y > j.y)
        {
            ret = true;
        }
    }
    return ret;
}


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state *state = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        HHXCB_ASSERT(sizeof(tilemap_position) == 4);
        HHXCB_ASSERT(sizeof(world_position) == 16);

        printf("Initializing memory\n");
        *state = {};
        state->wav = {};
        state->tilemap = {};
        state->tilemap.chunk_dim = 16;
        state->tilemap.tile_side_in_pixels = 64;
        state->tilemap.tile_side_in_meters = 1.4f;
        state->tilemap.meters_to_pixels = state->tilemap.tile_side_in_pixels / state->tilemap.tile_side_in_meters;
        state->tilemap.chunk_count_x = 16;
        state->tilemap.chunk_count_y = 16;
        uint32 total_chunks = state->tilemap.chunk_count_x * state->tilemap.chunk_count_y;

        state->tilemap.chunks = (tilemap_chunk *)calloc(sizeof(tilemap_chunk), total_chunks);
        populate_tilemap(state, &state->tilemap);

        state->objects_bitset = 0;
        state->bmps_bitset = 0;

        char filename[1024];
        find_data_file((char *)"Loop-Menu.wav", sizeof(filename), filename);
        wav_open(filename, &state->wav);

        find_data_file((char *)"KenneyRPGpack.bmp", sizeof(filename), filename);
        bmp_open(filename, &state->tilemap.tile_set.bmp);
        state->tilemap.tile_set.width = 64;
        state->tilemap.tile_set.height = 64;

        Memory->IsInitialized = true;

        {

            bmp_data *b = BMP_ALLOCATE(state);
            find_data_file((char *)"goblinsword.bmp", sizeof(filename), filename);
            bmp_open(filename, b);

            sprite_data *s = SPRITE_ALLOCATE(state);
            s->bmp = b;
            s->bmp_height = b->height;
            s->bmp_width = b->width;

            {
                game_object *g = GAME_OBJECT_ALLOCATE(state);
                g->sprite = s;
                g->location.tile_x = 2;
                g->location.tile_y = 2;
                g->location.x = 0;
                g->location.y = 0;
                state->player = g;
            }

            {
                game_object *g = GAME_OBJECT_ALLOCATE(state);
                g->sprite = s;
                g->location.tile_x = 3;
                g->location.tile_y = 3;
                g->location.x = 0;
                g->location.y = 0;
                state->player2 = g;
            }
        }

        {
            bmp_data *b = BMP_ALLOCATE(state);
            find_data_file((char *)"KenneyRPGpack.bmp", sizeof(filename), filename);
            bmp_open(filename, b);

            sprite_data *s = SPRITE_ALLOCATE(state);

            s->bmp = b;
            s->bmp_height = 64;
            s->bmp_width = 64;
            s->bmp_x = (175 * 64) % b->width;
            s->bmp_y = ((175 * 64) / b->width) * 64;
            s->bmp_offset_x = 0;
            s->bmp_offset_y = 64;

            game_object *g = GAME_OBJECT_ALLOCATE(state);
            g->sprite = s;
            g->location.tile_x = 4;
            g->location.tile_y = 4;
            g->location.x = 0;
            g->location.y = 0;

            g = GAME_OBJECT_ALLOCATE(state);
            g->sprite = s;
            g->location.tile_x = 6;
            g->location.tile_y = 3;
            g->location.x = 0;
            g->location.y = 0;

            s = SPRITE_ALLOCATE(state);

            s->bmp = b;
            s->bmp_height = 64;
            s->bmp_width = 64;
            s->bmp_x = (195 * 64) % b->width;
            s->bmp_y = ((195 * 64) / b->width) * 64;
            s->bmp_offset_x = 0;
            s->bmp_offset_y = 0;

            g = GAME_OBJECT_ALLOCATE(state);
            g->sprite = s;
            g->location.tile_x = 4;
            g->location.tile_y = 4;
            g->location.x = 0;
            g->location.y = 0;

            g = GAME_OBJECT_ALLOCATE(state);
            g->sprite = s;
            g->location.tile_x = 6;
            g->location.tile_y = 3;
            g->location.x = 0;
            g->location.y = 0;
        }
    }

    alternate_debug = 0;

    for(int index = 0;
        index < 5;
        ++index)
    {
        real32 dPlayerX = 0.0f;
        real32 dPlayerY = 0.0f;
        real32 dPlayer2X = 0.0f;
        real32 dPlayer2Y = 0.0f;

        game_controller_input *Controller = &Input->Controllers[index];

        dPlayerX += Controller->StickAverageX;
        dPlayerY -= Controller->StickAverageY;

        if(Controller->MoveUp.EndedDown)
        {
            dPlayer2Y = 1.0f;
        }
        if(Controller->MoveDown.EndedDown)
        {
            dPlayer2Y = -1.0f;
        }
        if(Controller->MoveLeft.EndedDown)
        {
            dPlayer2X = -1.0f;
        }
        if(Controller->MoveRight.EndedDown)
        {
            dPlayer2X = 1.0f;
        }
        if(Controller->ActionUp.EndedDown)
        {
            Memory->IsInitialized = false;
            printf("Set memory to be reinitialized on next frame\n");
        }
        if(Controller->ActionRight.EndedDown)
        {
            alternate_debug = 1;
        }
        if(Controller->RightShoulder.EndedDown)
        {
            dPlayerX *= 4.0f;
            dPlayerY *= 4.0f;
        }

        dPlayerX *= 4.0f;
        dPlayerY *= 4.0f;
        dPlayer2X *= 4.0f;
        dPlayer2Y *= 4.0f;

        world_position test_positions[3] = {
            state->player->location,
            state->player->location,
            state->player->location,
        };
        bool32 move_impossible = 0;
        for (uint i = 0;
                i < 3;
                ++i)
        {
            world_position *t = &test_positions[i];
            t->x += (Input->dtForFrame * dPlayerX);
            t->y += (Input->dtForFrame * dPlayerY);
            t->x += -.30f + (.30f * i);
            recanonicalize(&state->tilemap, t);
            uint32 tile_value = get_tile_value(&state->tilemap, t->tile_x, t->tile_y);
            tile_value &= 0xFF;
            if (tile_value != 39)
            {
                printf("try #%d: tile_x: %d, tile_y: %d,\n", i, t->tile_x, t->tile_y);
                printf("tile_value: %d\n", tile_value);
                move_impossible = 1;
                break;
            }
        }

        if (!move_impossible)
        {
            state->player->location.x += (Input->dtForFrame * dPlayerX);
            state->player->location.y += (Input->dtForFrame * dPlayerY);
        }


        state->player2->location.x += (Input->dtForFrame * dPlayer2X);
        state->player2->location.y += (Input->dtForFrame * dPlayer2Y);
    }

    recanonicalize(&state->tilemap, &state->player->location);

    bzero(Buffer->Memory, Buffer->Pitch * Buffer->Height);

    game_object_list gl[64] = {};
    game_object_list *head = gl;
    game_object_list *temp = gl;

    game_object *iterator = 0;
    uint count = 0;
    for(;;)
    {
        iterate_bitset_array(state->objects, state->objects_bitset, sizeof(game_object), (void **)&iterator);
        if (!iterator)
        {
            break;
        }

        if (alternate_debug)
        {
            game_object_list *t = head;
            printf("Starting with list: ");
            while (t && t->object)
            {
                printf("%dx%d|%.03fx%.03f, ", t->object->location.tile_x,
                        t->object->location.tile_y, t->object->location.x,
                        t->object->location.y);
                t = t->next;
            }
            printf(".\n");
            printf("Adding item: %dx%d|%.03fx%.03f\n",
                    iterator->location.tile_x, iterator->location.tile_y,
                    iterator->location.x, iterator->location.y);
        }


        temp->object = iterator;
        if (head == temp)
        {
            // Inserting first node
            // Already set object above...
        }
        else if (location_greater(iterator->location, head->object->location))
        {
            temp->next = head;
            head = temp;
        }
        else if (!head->next)
        {
            // Inserting second node
            if (!location_greater(iterator->location, head->object->location))
            {
                head->next = temp;
            }
        }
        else
        {
            game_object_list *current = head;
            while (current->next->next && !location_greater(iterator->location, current->next->object->location))
            {
                current = current->next;
            }

            if (!location_greater(iterator->location, current->next->object->location))
            {
                current->next->next = temp;
            }
            else if (!location_greater(iterator->location, current->object->location))
            {
                temp->next = current->next;
                current->next = temp;
            }
            else
            {
                printf("iterator->location.y is %.2f\n", iterator->location.y);
                printf("current->object->location.y is %.2f\n", current->object->location.y);
                printf("current->next->object->location.y is %.2f\n", current->next->object->location.y);
            }
        }
        temp++;
        count++;

        if (alternate_debug)
        {
            game_object_list *t = head;
            printf("Ending with list: ");
            while (t && t->object)
            {
                printf("%dx%d|%.03fx%.03f, ", t->object->location.tile_x,
                        t->object->location.tile_y, t->object->location.x,
                        t->object->location.y);
                t = t->next;
            }
            printf(".\n");
        }
    }

    draw_screen(Buffer, state, head);
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
