#include "../handmade_platform.h"

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../alternate/wav.h"
#include "../alternate/wav.cpp"
#include "../alternate/bmp.h"
#include "../alternate/bmp.cpp"

struct game_state
{
    wav_data wav;
    bmp_data bmp;

    real32 bmp_x;
    real32 bmp_y;
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

void
draw_bmp(game_offscreen_buffer *buffer, bmp_data *data, int x, int y)
{
    int max_x = x + data->width;
    int max_y = y + data->height;

    if (max_y > buffer->Height)
    {
        max_y = buffer->Height;
    }

    uint16 row_length = data->width * (data->bits_per_pixel / 8);
    uint8 *first_row = (uint8 *)data->bmp_memory;
    uint8 *final_row = first_row + (row_length * (data->height - 1));

    uint8 *row;
    uint8 *end_row;
    int8 direction;

    if (data->top_to_bottom)
    {
        direction = 1;
        row = first_row;
        end_row = final_row;
    }
    else
    {
        direction = -1;
        row = final_row;
        end_row = first_row;
    }

    uint8 *buffer_row = (uint8 *)buffer->Memory + (y * buffer->Pitch);
    while (y < max_y)
    {
        if (y > buffer->Height)
        {
            break;
        }
        uint8 *next_buffer_row = buffer_row + buffer->Pitch;
        uint8 *next_bmp_row = row + (row_length * direction);
        uint32 *buffer_pixel = (uint32 *)buffer_row;
        uint32 *bmp_row_pixel = (uint32 *)row;
        uint32 *bmp_final_row_pixel = bmp_row_pixel + data->width;

        if (x > 0)
        {
            buffer_pixel += x;
        }
        else if (x < 0)
        {
            bmp_row_pixel -= x;
        }

        if (y >= 0) {
            while(bmp_row_pixel < bmp_final_row_pixel && buffer_pixel < (uint32 *)next_buffer_row)
            {
                *buffer_pixel++ = *bmp_row_pixel++;
            }
        }

        buffer_row = next_buffer_row;
        row = next_bmp_row;;
        ++y;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state *state = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        state->wav = {};
        state->bmp = {};
        char filename[1024];
        find_data_file((char *)"Loop-Menu.wav", sizeof(filename), filename);
        wav_open(filename, &state->wav);

        find_data_file((char *)"goblinsword.bmp", sizeof(filename), filename);
        bmp_open(filename, &state->bmp);

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
        }

        dPlayerX *= 256.0f;
        dPlayerY *= 256.0f;

        state->bmp_x += (Input->dtForFrame * dPlayerX);
        state->bmp_y += (Input->dtForFrame * dPlayerY);
    }

    bzero(Buffer->Memory, Buffer->Pitch * Buffer->Height);
    draw_bmp(Buffer, &state->bmp, state->bmp_x, state->bmp_y);
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
