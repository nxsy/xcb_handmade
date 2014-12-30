#include "../handmade_platform.h"

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../alternate/wav.h"
#include "../alternate/wav.cpp"
#include "../alternate/bmp.h"
#include "../alternate/bmp.cpp"

struct tileset
{
    uint width;
    uint height;
    bmp_data bmp;
};

struct tilemap
{
    int16 tile_number;
    uint8 inverted;
};

struct tilescene {
    tileset tiles;
    uint8 width;
    uint8 height;
    uint8 num_tilemaps_under;
    uint8 num_tilemaps_over;
    tilemap *under_maps;
    tilemap *over_maps;
};

struct game_state
{
    wav_data wav;
    bmp_data bmp;
    tilescene scene;

    real32 bmp_x;
    real32 bmp_y;
};

#define DRAW_BMP_INVERTED 1

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

void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int buffer_x, int buffer_y, int bmp_x, int bmp_y, int bmp_width, int bmp_height, int flags)
{
    bool32 draw_inverted = flags & DRAW_BMP_INVERTED;
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

    if (draw_inverted > 0)
    {
        bmp_row += (bmp_pitch * bmp_direction * (bmp_height - 1));
        bmp_direction = -bmp_direction;
    }

    uint8 *buffer_row = (uint8 *)buffer->Memory + (buffer_y * buffer->Pitch);
    while (buffer_y < buffer_max_y)
    {
        if (buffer_y > buffer->Height)
        {
            printf("wtf?\n");
            break;
        }
        uint8 *next_buffer_row = buffer_row + buffer->Pitch;

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
            while(bmp_row_pixel < bmp_final_row_pixel && buffer_pixel < (uint32 *)next_buffer_row)
            {
                uint8 *subpixel = (uint8 *)bmp_row_pixel;
                uint8 *alpha = subpixel + 3;
                if (*alpha == 255) {
                    *buffer_pixel = *bmp_row_pixel;
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

void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int buffer_x, int buffer_y, int bmp_x, int bmp_y, int bmp_width, int bmp_height)
{
    draw_bmp(buffer, data, buffer_x, buffer_y, bmp_x, bmp_y, bmp_width, bmp_height, 0);
}

void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int x, int y)
{
    draw_bmp(buffer, data, x, y, 0, 0, data->width, data->height);
}

void
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

void
draw_tile(game_offscreen_buffer *buffer, tileset *t, int tilenumber, int x, int y)
{
    draw_tile(buffer, t, tilenumber, x, y, 0);
}

void draw_tilemap(game_offscreen_buffer *buffer, tilescene *scene, tilemap *map, int x, int y)
{
    for (uint row = 0;
            row < scene->height;
            ++row)
    {
        int row_offset = row * scene->width;

        for (uint column = 0;
                column < scene->width;
                ++column)
        {
            int flags = 0;
            if (map[row_offset+column].inverted)
            {
                flags |= DRAW_BMP_INVERTED;
            }
            draw_tile(buffer, &scene->tiles, map[row_offset+column].tile_number, x + (column*scene->tiles.width), y, flags);
        }
        y += scene->tiles.height;
    }
}

void draw_scene(game_offscreen_buffer *buffer, tilescene *scene, int x, int y)
{
    draw_tilemap(buffer, scene, &scene->under_maps[0], x, y);
    draw_tilemap(buffer, scene, &scene->under_maps[scene->width * scene->height], x, y);
}

void draw_scene_over(game_offscreen_buffer *buffer, tilescene *scene, int x, int y)
{
    draw_tilemap(buffer, scene, &scene->over_maps[0], x, y);
}

void
set_tile(tilescene *scene, tilemap *tilemap, int tilemap_number, int x, int y, int tile_number)
{
    tilemap[(tilemap_number * scene->height * scene->width) + (y * scene->width) + x].tile_number = tile_number;
}

void
populate_scene(tilescene *scene)
{
    scene->tiles.height = 64;
    scene->height = 9;
    scene->tiles.width = 64;
    scene->width = 15;
    scene->num_tilemaps_under = 2;
    scene->num_tilemaps_over = 2;
    if (scene->under_maps)
    {
        free(scene->under_maps);
    }
    scene->under_maps = (tilemap *)calloc(sizeof(tilemap), scene->height * scene->width * scene->num_tilemaps_under);
    if (scene->over_maps)
    {
        free(scene->over_maps);
    }
    scene->over_maps = (tilemap *)calloc(sizeof(tilemap), scene->height * scene->width * scene->num_tilemaps_over);

    uint i = 0;

    scene->under_maps[i++].tile_number = 13;
    while (i < scene->width - 1)
    {
        scene->under_maps[i++].tile_number = 45;
    }
    scene->under_maps[i++].tile_number = 14;

    for (;
            i < scene->width * (scene->height - 1);
            ++i)
    {
        if (i % scene->width == 0)
        {
            scene->under_maps[i].tile_number = 30;
        }
        else if ((i + 1) % scene->width == 0)
        {
            scene->under_maps[i].tile_number = 28;
        }
        else
        {
            scene->under_maps[i].tile_number = 39;
        }
    }

    scene->under_maps[i++].tile_number = 31;
    while (i < (scene->width * scene->height - 1))
    {
        scene->under_maps[i].inverted = 1;
        scene->under_maps[i++].tile_number = 45;
    }
    scene->under_maps[i++].tile_number = 32;

    tilemap *under_map2 = &scene->under_maps[scene->width * scene->height];
    for (int j = 0;
            j < (scene->width * scene->height);
            ++j)
    {
        under_map2[j].tile_number = -1;
    }

    tilemap *over_map = scene->over_maps;
    for (int j = 0;
            j < (scene->width * scene->height);
            ++j)
    {
        over_map[j].tile_number = -1;
    }

    set_tile(scene, scene->over_maps, 0, 4, 4, 175);
    set_tile(scene, scene->under_maps, 1, 4, 5, 195);

    set_tile(scene, scene->over_maps, 0, 5, 4, 175);
    set_tile(scene, scene->under_maps, 1, 5, 5, 195);

    set_tile(scene, scene->over_maps, 0, 3, 3, 175);
    set_tile(scene, scene->under_maps, 1, 3, 4, 195);

    set_tile(scene, scene->over_maps, 0, 3, 6, 175);
    set_tile(scene, scene->under_maps, 1, 3, 7, 195);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state *state = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        state->wav = {};
        state->bmp = {};
        state->scene = {};
        char filename[1024];
        find_data_file((char *)"Loop-Menu.wav", sizeof(filename), filename);
        wav_open(filename, &state->wav);

        find_data_file((char *)"goblinsword.bmp", sizeof(filename), filename);
        bmp_open(filename, &state->bmp);

        find_data_file((char *)"KenneyRPGpack.bmp", sizeof(filename), filename);
        bmp_open(filename, &state->scene.tiles.bmp);

        populate_scene(&state->scene);

        Memory->IsInitialized = true;

        state->bmp_x = 200;
        state->bmp_y = 200;
    }

    for(int index = 0;
        index < 5;
        ++index)
    {
        real32 dPlayerX = 0.0f;
        real32 dPlayerY = 0.0f;

        game_controller_input *Controller = &Input->Controllers[index];

        if(Controller->IsAnalog)
        {
            dPlayerX += Controller->StickAverageX;
            dPlayerY += Controller->StickAverageY;
        }
        else {

            if(Controller->MoveUp.EndedDown)
            {
                dPlayerY = -1.0f;
            }
            if(Controller->MoveDown.EndedDown)
            {
                dPlayerY = 1.0f;
            }
            if(Controller->MoveLeft.EndedDown)
            {
                dPlayerX = -1.0f;
            }
            if(Controller->MoveRight.EndedDown)
            {
                dPlayerX = 1.0f;
            }
            if(Controller->ActionDown.EndedDown)
            {
                populate_scene(&state->scene);
            }
        }

        dPlayerX *= 256.0f;
        dPlayerY *= 256.0f;

        state->bmp_x += (Input->dtForFrame * dPlayerX);
        state->bmp_y += (Input->dtForFrame * dPlayerY);
    }

    bzero(Buffer->Memory, Buffer->Pitch * Buffer->Height);
    draw_scene(Buffer, &state->scene, 0, -16);
    draw_bmp(Buffer, &state->bmp, state->bmp_x, state->bmp_y);
    draw_scene_over(Buffer, &state->scene, 0, -16);
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
