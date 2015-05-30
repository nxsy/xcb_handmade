/*
 * Copyright (c) 2014, Neil Blakey-Milner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/mman.h> // mmap, PROT_*, MAP_*
#include <sys/stat.h> // stat, fstat

#include <dirent.h>   // DIR *, opendir
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <errno.h>    // errno
#include <fcntl.h>    // open, O_RDONLY
#include <stdio.h>    // printf
#include <stdlib.h>   // malloc, calloc, free
#include <string.h>   // strlen (I'm so lazy)
#include <time.h>     // clock_gettime, CLOCK_MONOTONIC
#include <unistd.h>   // readlink
#include <pthread.h>  // pthread_create, pthread_attr_init
#include <semaphore.h>// sem_init, sem_post, sem_wait, sem_destroy
// Note: this gives a compiler error for "_Atomic"
//#include <stdatomic.h>// atomic_fetch_add, etc...
	
#include <xcb/xcb.h>

/*
 * Some versions of this header use class as a function argument name, which
 * C++ does not like...
 */
#define class class_name
#include <xcb/xcb_icccm.h>
#undef class

#include <xcb/xcb_image.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <linux/joystick.h>

#include <alsa/asoundlib.h>

#include "handmade_platform.h"
#include "xcb_handmade.h"

#define internal static
#define local_persist static
#define global_variable static
#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#ifndef GAME_CODE_FILENAME
#define GAME_CODE_FILENAME libhandmade.so
#endif
#define _HHXCB_QUOTE2(X) #X
#define _HHXCB_QUOTE(X) _HHXCB_QUOTE2(X)

internal real32
hhxcb_process_controller_axis(int16 value, uint16 dead_zone, bool inverted)
{
    if (inverted)
    {
        value = -value;
    }
    if (abs(value) < dead_zone)
    {
        return 0;
    }
    else if (value < -dead_zone)
    {
        return (real32)((value + dead_zone) / (32767.0f - dead_zone));
    }
    return (real32)((value - dead_zone) / (32767.0f - dead_zone));
}

internal void
hhxcb_process_button(bool down, game_button_state *old_state, game_button_state *new_state)
{
    new_state->EndedDown = down;
    new_state->HalfTransitionCount = (old_state->EndedDown != new_state->EndedDown) ? 1 : 0;
}

xcb_format_t *hhxcb_find_format(hhxcb_context *context, uint32_t pad, uint32_t depth, uint32_t bpp)
{
    xcb_format_t *fmt = xcb_setup_pixmap_formats(context->setup);
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(context->setup);
    while (fmt++ != fmtend)
    {
        if (fmt->scanline_pad == pad && fmt->depth == depth && fmt->bits_per_pixel == bpp)
        {
            return fmt;
        }
    }
    return (xcb_format_t *)0;
}

xcb_image_t *hhxcb_create_image(uint32_t width, uint32_t height, xcb_format_t *fmt, const xcb_setup_t *setup)
{
    size_t image_size = width * height * (fmt->bits_per_pixel / 8);
    uint8_t *image_data = (uint8_t *)malloc(image_size);

     return xcb_image_create(width, height, XCB_IMAGE_FORMAT_Z_PIXMAP,
             fmt->scanline_pad, fmt->depth, fmt->bits_per_pixel, 0,
             (xcb_image_order_t)setup->image_byte_order,
             XCB_IMAGE_ORDER_LSB_FIRST, image_data, image_size, image_data);
}

internal void
hhxcb_resize_backbuffer(hhxcb_context *context, hhxcb_offscreen_buffer *buffer, int width, int height)
{
    // xcb_image owns the referenced data, so will clean it up without an explicit free()
    if (buffer->xcb_image)
    {
        xcb_image_destroy(buffer->xcb_image);
    }

    if (buffer->xcb_pixmap_id)
    {
        xcb_free_pixmap(context->connection, buffer->xcb_pixmap_id);
    }

    if (buffer->xcb_gcontext_id)
    {
        xcb_free_gc(context->connection, buffer->xcb_gcontext_id);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = context->fmt->bits_per_pixel / 8;
    buffer->pitch = buffer->bytes_per_pixel * width;

    buffer->xcb_image = hhxcb_create_image(width, height, context->fmt,
            context->setup);
    buffer->memory = buffer->xcb_image->data;

    buffer->xcb_pixmap_id = xcb_generate_id(context->connection);
    xcb_create_pixmap(context->connection, context->fmt->depth,
            buffer->xcb_pixmap_id, context->window, width, height);

    buffer->xcb_gcontext_id = xcb_generate_id (context->connection);
    xcb_create_gc (context->connection, buffer->xcb_gcontext_id,
            buffer->xcb_pixmap_id, 0, 0);
}

internal void
load_and_set_cursor(hhxcb_context *context)
{
    context->cursor_pixmap_id = xcb_generate_id(context->connection);
    xcb_create_pixmap(context->connection, /*depth*/ 1,
            context->cursor_pixmap_id, context->window, /*width*/ 1,
            /*height*/ 1);

    context->cursor_id = xcb_generate_id(context->connection);
    xcb_create_cursor(context->connection, context->cursor_id,
            context->cursor_pixmap_id, context->cursor_pixmap_id, 0, 0, 0, 0,
            0, 0, 0, 0);

    uint32_t values[1] = { context->cursor_id };
    xcb_change_window_attributes( context->connection, context->window,
            XCB_CW_CURSOR, values);
}

internal void
load_atoms(hhxcb_context *context)
{
    xcb_intern_atom_cookie_t wm_delete_window_cookie =
        xcb_intern_atom(context->connection, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie =
        xcb_intern_atom(context->connection, 0, strlen("WM_PROTOCOLS"),
                "WM_PROTOCOLS");

    xcb_flush(context->connection);
    xcb_intern_atom_reply_t* wm_delete_window_cookie_reply =
        xcb_intern_atom_reply(context->connection, wm_delete_window_cookie, 0);
    xcb_intern_atom_reply_t* wm_protocols_cookie_reply =
        xcb_intern_atom_reply(context->connection, wm_protocols_cookie, 0);

    context->wm_protocols = wm_protocols_cookie_reply->atom;
    context->wm_delete_window = wm_delete_window_cookie_reply->atom;
}

#ifdef HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_xcb_free_file_memory)
{
    free(Memory);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_xcb_read_entire_file)
{
    debug_read_file_result result = {};

    FILE *f = fopen(Filename, "rb");

    if (f == 0)
    {
        printf("Failed to open file %s\n", Filename);
        return result;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    result.Contents = string;
    result.ContentsSize = fsize;

    return(result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_xcb_write_entire_file)
{
    FILE *f = fopen(Filename, "wb");

    if (f == 0)
    {
        printf("Failed to open file %s\n", Filename);
        return 0;
    }

    size_t res = fwrite(Memory, 1, MemorySize, f);
    fclose(f);

    return res == MemorySize;
}

#endif

internal void
HandleDebugCycleCounter(game_memory* m)
{
#ifdef HANDMADE_INTERNAL
	printf("DEBUG CYCLE COUNTS:\n");
	
	for(uint CounterIndex =0;
		CounterIndex < ArrayCount(m->Counters);
		++CounterIndex)
	{
		debug_cycle_counter* Counter = m->Counters + CounterIndex;
		if(Counter->HitCount)
		{
			printf("  %d: %lucy %uh %lucy/h\n", CounterIndex,
				   Counter->CycleCount, Counter->HitCount, 
				   (uint64)(Counter->CycleCount / Counter->HitCount));
			Counter->HitCount = 0;
			Counter->CycleCount = 0;
		}
		fflush(stdout);
	}
#endif
}


internal void
hhxcb_get_binary_name(hhxcb_state *state)
{
    // NOTE(nbm): There are probably pathological cases where this won't work - for
    // example if the underlying file has been removed/moved.
    readlink("/proc/self/exe", state->binary_name, HHXCB_STATE_FILE_NAME_LENGTH);
    for (char *c = state->binary_name; *c; ++c)
    {
        if (*c == '/')
        {
            state->one_past_binary_filename_slash = c + 1;
        }
    }
}

internal void
hhxcb_cat_strings(
    size_t src_a_count, char *src_a,
    size_t src_b_count, char *src_b,
    size_t dest_count, char *dest
    )
{
    size_t counter = 0;
    for (
            size_t i = 0;
            i < src_a_count && counter++ < dest_count;
            ++i)
    {
        *dest++ = *src_a++;
    }
    for (
            size_t i = 0;
            i < src_b_count && counter++ < dest_count;
            ++i)
    {
        *dest++ = *src_b++;
    }

    *dest++ = 0;
}

internal void
hhxcb_build_full_filename(hhxcb_state *state, char *filename, int dest_count, char *dest)
{
    hhxcb_cat_strings(state->one_past_binary_filename_slash -
            state->binary_name, state->binary_name, strlen(filename), filename,
            dest_count, dest);
}

internal void
hhxcb_load_game(hhxcb_game_code *game_code, char *path)
{
    struct stat statbuf = {};
    uint32_t stat_result = stat(path, &statbuf);
    if (stat_result != 0)
    {
        printf("Failed to stat game code at %s", path);
        return;
    }
    game_code->library_mtime = statbuf.st_mtime;

    game_code->is_valid = false;
    game_code->library_handle = dlopen(path, RTLD_LAZY);
    if (game_code->library_handle == 0)
    {
        char *error = dlerror();
        printf("Unable to load library at path %s: %s\n", path, error);
        return;
    }
    game_code->UpdateAndRender =
        (game_update_and_render *)dlsym(game_code->library_handle, "GameUpdateAndRender");
    if (game_code->UpdateAndRender == 0)
    {
        char *error = dlerror();
        printf("Unable to load symbol GameUpdateAndRender: %s\n", error);
        free(error);
        return;
    }
    game_code->GetSoundSamples =
        (game_get_sound_samples *)dlsym(game_code->library_handle, "GameGetSoundSamples");
    if (game_code->UpdateAndRender == 0)
    {
        char *error = dlerror();
        printf("Unable to load symbol GameGetSoundSamples: %s\n", error);
        free(error);
        return;
    }

    game_code->is_valid = game_code->library_handle &&
        game_code->UpdateAndRender && game_code->GetSoundSamples;
}

internal void
hhxcb_unload_game(hhxcb_game_code *game_code)
{
    if (game_code->library_handle)
    {
        dlclose(game_code->library_handle);
        game_code->library_handle = 0;
    }
    game_code->is_valid = 0;
    game_code->UpdateAndRender = 0;
    game_code->GetSoundSamples = 0;
}

internal void
hhxcb_get_input_file_location(hhxcb_state *state, bool32 input_stream, uint index, int dest_size, char *dest)
{
    char temp[64];
    sprintf(temp, "loop_edit_%d_%s.hmi", index, input_stream ? "input" : "state");
    hhxcb_build_full_filename(state,
            temp,
            dest_size,
            dest);
}

internal hhxcb_replay_buffer *
hhxcb_get_replay_buffer(hhxcb_state *state, uint8 index)
{
    Assert(index < ArrayCount(state->replay_buffers));
    hhxcb_replay_buffer *result = &state->replay_buffers[index];
    return result;
}

internal void
hhxcb_init_replays(hhxcb_state *state)
{
    for (uint8 index = 0;
            index < ArrayCount(state->replay_buffers);
            ++index)
    {
        hhxcb_replay_buffer *replay_buffer = &state->replay_buffers[index];

        hhxcb_get_input_file_location(state, false, index,
                sizeof(replay_buffer->filename), replay_buffer->filename);

        replay_buffer->file_handle = open(replay_buffer->filename,
                O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        int truncate_succeeded = ftruncate(replay_buffer->file_handle, state->total_size);
        if (truncate_succeeded == -1)
        {
            perror("ftruncate");
        }

        replay_buffer->memory_block = mmap(0, state->total_size,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_POPULATE,
                replay_buffer->file_handle, 0);

        if (replay_buffer->memory_block == MAP_FAILED)
        {
            perror("mmap");
        }
    }
}

internal void
hhxcb_start_recording(hhxcb_state *state, uint8 index)
{
    hhxcb_replay_buffer *replay_buffer = hhxcb_get_replay_buffer(state, index);
    if (replay_buffer->memory_block)
    {
        state->recording_index = index;
        char filename[HHXCB_STATE_FILE_NAME_LENGTH];
        hhxcb_get_input_file_location(state, true, index, sizeof(filename), filename);
        state->recording_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        memcpy(replay_buffer->memory_block, state->game_memory_block, state->total_size);
    }
}

internal void
hhxcb_stop_recording(hhxcb_state *state)
{
    close(state->recording_fd);
    state->recording_index = 0;
    state->recording_fd = 0;
}

internal void
hhxcb_start_playback(hhxcb_state *state, uint8 index)
{
    hhxcb_replay_buffer *replay_buffer = hhxcb_get_replay_buffer(state, index);
    if (replay_buffer->memory_block)
    {
        state->playback_index = index;
        char filename[HHXCB_STATE_FILE_NAME_LENGTH];
        hhxcb_get_input_file_location(state, true, index, sizeof(filename), filename);
        state->playback_fd = open(filename, O_RDONLY);
        memcpy(state->game_memory_block, replay_buffer->memory_block, state->total_size);
    }
}

internal void
hhxcb_stop_playback(hhxcb_state *state)
{
    close(state->playback_fd);
    state->playback_index = 0;
    state->playback_fd = 0;
}

internal void
hhxcb_record_input(hhxcb_state *state, game_input *new_input)
{
    write(state->recording_fd, new_input, sizeof(*new_input));
}

internal void
hhxcb_playback_input(hhxcb_state *state, game_input *new_input)
{
    int bytes_read = read(state->playback_fd, new_input, sizeof(*new_input));
    if (bytes_read == 0)
    {
        uint8 index = state->playback_index;
        hhxcb_stop_playback(state);
        hhxcb_start_playback(state, index);
        read(state->playback_fd, new_input, sizeof(*new_input));
    }
}

internal void
hhxcb_process_keyboard_message(game_button_state *new_state, bool32 is_down)
{
    if (new_state->EndedDown != is_down)
    {
        new_state->EndedDown = is_down;
        ++new_state->HalfTransitionCount;
    }
}

internal void
hhxcb_process_events(hhxcb_context *context, hhxcb_state *state, game_input *new_input, game_input *old_input)
{
    game_controller_input *old_keyboard_controller = GetController(old_input, 0);
    game_controller_input *new_keyboard_controller = GetController(new_input, 0);
    *new_keyboard_controller = {};
    new_keyboard_controller->IsConnected = true;
    for (
            uint button_index = 0;
            button_index < ArrayCount(new_keyboard_controller->Buttons);
            ++button_index)
    {
        new_keyboard_controller->Buttons[button_index].EndedDown =
            old_keyboard_controller->Buttons[button_index].EndedDown;
    }

    new_input->MouseX = old_input->MouseX;
    new_input->MouseY = old_input->MouseY;

    for (int i = 0; i < HHXCB_MAX_CONTROLLERS; ++i)
    {
        hhxcb_controller_info *pad = &context->controller_info[i];
        if (!pad->is_active)
        {
            continue;
        }
        game_controller_input *old_controller = GetController(old_input, i+1);
        game_controller_input *new_controller = GetController(new_input, i+1);

        *new_controller = *old_controller;
        new_controller->IsConnected = true;
        new_controller->IsAnalog = old_controller->IsAnalog;

        js_event j;
        while (read(pad->fd, &j, sizeof(js_event)) == sizeof(js_event))
        {
            // Don't care if init or afterwards
            j.type &= ~JS_EVENT_INIT;
            if (j.type == JS_EVENT_BUTTON)
            {
                if (j.number == pad->a_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->ActionDown,
                            &new_controller->ActionDown);
                }
                else if (j.number == pad->b_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->ActionRight,
                            &new_controller->ActionRight);
                }
                else if (j.number == pad->x_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->ActionLeft,
                            &new_controller->ActionLeft);
                }
                else if (j.number == pad->y_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->ActionUp,
                            &new_controller->ActionUp);
                }
                else if (j.number == pad->l1_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->LeftShoulder,
                            &new_controller->LeftShoulder);
                }
                else if (j.number == pad->r1_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->RightShoulder,
                            &new_controller->RightShoulder);
                }
                else if (j.number == pad->back_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->Back,
                            &new_controller->Back);
                }
                else if (j.number == pad->start_button)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->Start,
                            &new_controller->Start);
                }
                else
                {
                    printf("Unhandled button: number %d, value %d\n",
                            j.number, j.value);
                }
            }
            if (j.type == JS_EVENT_AXIS)
            {
                uint16 dead_zone = pad->axis_dead_zones[j.number];
                bool axis_inverted = pad->axis_inverted[j.number];
                if (j.number == pad->left_thumb_x_axis)
                {
                    new_controller->StickAverageX =
                        hhxcb_process_controller_axis(j.value, dead_zone,
                                axis_inverted);
                    if (new_controller->StickAverageX != 0.0f)
                    {
                        new_controller->IsAnalog = true;
                    }
                }
                else if (j.number == pad->left_thumb_y_axis)
                {
                    new_controller->StickAverageY =
                        hhxcb_process_controller_axis(j.value, dead_zone,
                                axis_inverted);
                    if (new_controller->StickAverageY != 0.0f)
                    {
                        new_controller->IsAnalog = true;
                    }
                }
                else if (j.number == pad->right_thumb_x_axis)
                {
                    // TODO(nbm): Do something with this.  Here otherwise we get spammed.
                }
                else if (j.number == pad->right_thumb_y_axis)
                {
                    // TODO(nbm): Do something with this.  Here otherwise we get spammed.
                }
                else if (j.number == pad->dpad_x_axis)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->MoveRight,
                            &new_controller->MoveRight);
                    hhxcb_process_button((j.value < 0),
                            &old_controller->MoveLeft,
                            &new_controller->MoveLeft);
                    new_controller->IsAnalog = false;
                }
                else if (j.number == pad->dpad_y_axis)
                {
                    hhxcb_process_button((j.value > 0),
                            &old_controller->MoveDown,
                            &new_controller->MoveDown);
                    hhxcb_process_button((j.value < 0),
                            &old_controller->MoveUp,
                            &new_controller->MoveUp);
                    new_controller->IsAnalog = false;
                }
                else
                {
                    printf("Unhandled Axis: number %d, value %d\n",
                        j.number, j.value);
                }

            }
        }
        if (errno != EAGAIN) {
            context->need_controller_refresh = true;
        }
    }

    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(context->connection)))
    {
        // NOTE(nbm): The high-order bit of response_type is whether the event
        // is synthetic.  I'm not sure I care, but let's grab it in case.
        bool32 synthetic_event = (event->response_type & 0x80) != 0;
        uint8_t response_type = event->response_type & ~0x80;
        switch(response_type)
        {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE:
            {
                xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
                bool32 is_down = (response_type == XCB_KEY_PRESS);
                xcb_keysym_t keysym = xcb_key_symbols_get_keysym(context->key_symbols, e->detail, 0);

                if (keysym == XK_w)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->MoveUp, is_down);
                }
                else if (keysym == XK_a)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->MoveLeft, is_down);
                }
                else if (keysym == XK_s)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->MoveDown, is_down);
                }
                else if (keysym == XK_d)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->MoveRight, is_down);
                }
                else if (keysym == XK_q)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->LeftShoulder, is_down);
                }
                else if (keysym == XK_e)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->RightShoulder, is_down);
                }
                else if (keysym == XK_Up)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->ActionUp, is_down);
                }
                else if (keysym == XK_Left)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->ActionLeft, is_down);
                }
                else if (keysym == XK_Down)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->ActionDown, is_down);
                }
                else if (keysym == XK_Right)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->ActionRight, is_down);
                }
                else if (keysym == XK_Escape)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->Back, is_down);
                }
                else if (keysym == XK_space)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->Start, is_down);
                }
                else if (keysym == XK_l)
                {
                    if (is_down)
                    {
                        if (state->playback_index == 0)
                        {
                            if (state->recording_index == 0)
                            {
                                hhxcb_start_recording(state, 1);
                            }
                            else
                            {
                                hhxcb_stop_recording(state);
                                hhxcb_start_playback(state, 1);
                            }
                        }
                        else
                        {
                            hhxcb_stop_playback(state);
                        }
                    }
                }
                break;
            }
            case XCB_NO_EXPOSURE:
            {
                // No idea what these are, but they're spamming me.
                break;
            }
            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t* e = (xcb_motion_notify_event_t*)event;
                new_input->MouseX = e->event_x;
                new_input->MouseY = e->event_y;
                break;
            }
            case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t* client_message_event = (xcb_client_message_event_t*)event;

                if (client_message_event->type == context->wm_protocols)
                {
                    if (client_message_event->data.data32[0] == context->wm_delete_window)
                    {
                        context->ending_flag = 1;
                        break;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
        free(event);
    };
}

internal void
hhxcb_refresh_controllers(hhxcb_context *context)
{
    // TODO(nbm): At the end of the function we should remove any controllers
    // that remain with is_active=false, closing their fds, and so forth.
    for(uint i = 0; i < ArrayCount(context->controller_info); i++)
    {
        context->controller_info[i].is_active = false;
    }

    DIR *dir = opendir("/dev/input");
    dirent entry;
    dirent *result;
    while (!readdir_r(dir, &entry, &result) && result)
    {
        if ((entry.d_name[0] == 'j') && (entry.d_name[1] == 's'))
        {
            char full_device_path[HHXCB_STATE_FILE_NAME_LENGTH];
            snprintf(full_device_path, sizeof(full_device_path), "%s/%s", "/dev/input", entry.d_name);
            bool found = false;
            for(uint i = 0; i < ArrayCount(context->controller_info); i++)
            {
                if (strcmp(context->controller_info[i].path, full_device_path) == 0)
                {
                    context->controller_info[i].is_active = true;
                    found = true;
                }
            }
            if (found) {
                continue;
            }

            int fd = open(full_device_path, O_RDONLY);
            if (fd < 0)
            {
                // TODO(nbm): log or something - permission may prevent open/read
                continue;
            }

            char name[128];
            ioctl(fd, JSIOCGNAME(128), name);
            if (!strstr(name, "Microsoft X-Box 360 pad"))
            {
                // TODO(nbm): log or something
                close(fd);
                continue;
            }

            hhxcb_controller_info *use = 0;
            for(uint i = 0; i < ArrayCount(context->controller_info); i++)
            {
                if (context->controller_info[i].fd <= 0)
                {
                    use = &context->controller_info[i];
                    break;
                }
            }
            if (!use) {
                // TODO(nbm): log
                close(fd);
                continue;
            }

            // Does this ever matter?
            int version;
            ioctl(fd, JSIOCGVERSION, &version);
            uint8 axes;
            ioctl(fd, JSIOCGAXES, &axes);
            uint8 buttons;
            ioctl(fd, JSIOCGBUTTONS, &buttons);
            printf("%s: %s, v%u, axes: %u, buttons: %u\n", entry.d_name, name, version, axes, buttons);

            snprintf(use->path, sizeof(use->path), "%s/%s", "/dev/input", entry.d_name);
            use->is_active = true;
            use->fd = fd;
            use->axis_dead_zones[0] = 7849;
            use->axis_dead_zones[1] = 7849;
            use->left_thumb_x_axis = 0;
            use->left_thumb_y_axis = 1;
            use->right_thumb_x_axis = 3;
            use->right_thumb_y_axis = 4;
            use->a_button = 0;
            use->b_button = 1;
            use->x_button = 2;
            use->y_button = 3;
            use->l1_button = 4;
            use->r1_button = 5;
            use->back_button = 6;
            use->start_button = 7;
            use->dpad_x_axis = 6;
            use->dpad_y_axis = 7;
            fcntl(fd, F_SETFL, O_NONBLOCK);
        }
    }
    closedir(dir);
}

internal
void hhxcb_init_alsa(hhxcb_context *context, hhxcb_sound_output *sound_output)
{
    // NOTE: "hw:0,0" doesn't seem to work with alsa running on top of pulseaudio
    char *device = (char *)"default";
    // char *device = (char *)"hw:0,0";

    int err;
    snd_pcm_sframes_t frames;
    err = snd_output_stdio_attach(&context->alsa_log, stderr, 0);

    if ((err = snd_pcm_open(&context->handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
            printf("Playback open error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    snd_pcm_hw_params_any(context->handle, hwparams);
    snd_pcm_hw_params_set_rate_resample(context->handle, hwparams, 0);
    snd_pcm_hw_params_set_access(context->handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(context->handle, hwparams, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(context->handle, hwparams, 2);
    snd_pcm_hw_params_set_rate(context->handle, hwparams, sound_output->samples_per_second, 0);
    snd_pcm_hw_params_set_period_size(context->handle, hwparams, sound_output->samples_per_second / 60, 0);
    // NOTE: restricting this buffer size too much seems to crash the game
    sound_output->secondary_buffer_size = 48000 / 2;
    snd_pcm_hw_params_set_buffer_size(context->handle, hwparams, sound_output->secondary_buffer_size);
    snd_pcm_hw_params(context->handle, hwparams);
    snd_pcm_dump(context->handle, context->alsa_log);
}

struct work_queue_entry_storage
{
	void* UserPointer;
};

struct work_queue
{
	uint32 volatile EntryCompletionCount;
	uint32 volatile NextEntryToDo;
	uint32 volatile EntryCount;

	sem_t* SemaphoreHandle;
	
	work_queue_entry_storage Entries[256];
};

struct work_queue_entry
{
	void* Data;
	bool32 IsValid;
};

internal void
AddWorkQueueEntry(work_queue* Queue, void* Pointer)
{
	Assert(Queue->EntryCount < ArrayCount(Queue->Entries));
	Queue->Entries[Queue->EntryCount].UserPointer = Pointer;
	_mm_sfence();
	++Queue->EntryCount;
	sem_post(Queue->SemaphoreHandle);
}


internal work_queue_entry
CompleteAndGetNextWorkQueueEntry(work_queue* Queue, work_queue_entry Completed)
{
	work_queue_entry Result;
	Result.IsValid = false;

	if(Completed.IsValid)
	{
		__sync_fetch_and_add(&Queue->EntryCompletionCount, 1);
	}
	
	if(Queue->NextEntryToDo < Queue->EntryCount)
	{
		// NOTE: apparently the "__sync*" compiler intrinsics are
		// supported in gcc and clang
		uint32 Index = __sync_fetch_and_add(&Queue->NextEntryToDo, 1);
		Result.Data = Queue->Entries[Index].UserPointer;
		Result.IsValid = true;
		
		_mm_lfence();
	}
	return(Result);
}

internal bool32
QueueWorkStillInProgress(work_queue* Queue)
{
	bool32 Result = (Queue->EntryCount != Queue->EntryCompletionCount);
	return(Result);
}

inline void
DoWorkerWork(work_queue_entry Entry, uint32 LogicalThreadIndex)
{
	Assert(Entry.IsValid);
	
	printf("thread %u: %s\n", LogicalThreadIndex,
		   (char*)Entry.Data);
}

struct hhxcb_thread_info
{
	uint32 LogicalThreadIndex;
	work_queue* Queue;
};

void*
ThreadFunction(void* arg)
{
	hhxcb_thread_info* ThreadInfo = (hhxcb_thread_info*)arg;

	work_queue_entry Entry = {};
	for(;;)
	{
		Entry = CompleteAndGetNextWorkQueueEntry(ThreadInfo->Queue, Entry);
		if(Entry.IsValid)
		{
			DoWorkerWork(Entry, ThreadInfo->LogicalThreadIndex);
		}
		else
		{
			sem_wait(ThreadInfo->Queue->SemaphoreHandle);
		}
	}
}

internal void
PushString(work_queue* Queue, char* String)
{
	AddWorkQueueEntry(Queue, String);
}

int
main()
{

    hhxcb_state state = {};
    hhxcb_get_binary_name(&state);

    char source_game_code_library_path[HHXCB_STATE_FILE_NAME_LENGTH];
    char *game_code_filename = (char *) _HHXCB_QUOTE(GAME_CODE_FILENAME);
    hhxcb_build_full_filename(&state, game_code_filename,
            sizeof(source_game_code_library_path),
            source_game_code_library_path);
    hhxcb_game_code game_code = {};
    hhxcb_load_game(&game_code, source_game_code_library_path);

    hhxcb_context context = {};

    /* Open the connection to the X server. Use the DISPLAY environment variable */
    int screenNum;
    context.connection = xcb_connect (NULL, &screenNum);

    context.key_symbols = xcb_key_symbols_alloc(context.connection);

    /*
     * TODO(nbm): This is X-wide, so it really isn't a good option in reality.
     * We have to be careful and clean up at the end.  If we crash, auto-repeat
     * is left off.
     */
    {
        uint32_t values[1] = {XCB_AUTO_REPEAT_MODE_OFF};
        xcb_change_keyboard_control(context.connection, XCB_KB_AUTO_REPEAT_MODE, values);
    }

    load_atoms(&context);
    context.setup = xcb_get_setup(context.connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(context.setup);
    xcb_screen_t *screen = iter.data;
    context.fmt = hhxcb_find_format(&context, 32, 24, 32);

    int monitor_refresh_hz = 60;
    real32 game_update_hz = (monitor_refresh_hz / 2.0f); // Should almost always be an int...
    long target_nanoseconds_per_frame = (1000 * 1000 * 1000) / game_update_hz;


    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] =
    {
        0x0000ffff, //screen->black_pixel,
        0
            | XCB_EVENT_MASK_POINTER_MOTION
            | XCB_EVENT_MASK_KEY_PRESS
            | XCB_EVENT_MASK_KEY_RELEASE
            ,
    };

#define START_WIDTH 960
#define START_HEIGHT 540

    context.window = xcb_generate_id(context.connection);
	// NOTE: changed to not have a border width, so the min/max/close
	// buttons align on compiz, maybe other window managers
    xcb_create_window(context.connection, XCB_COPY_FROM_PARENT, context.window,
            screen->root, 0, 0, START_WIDTH, START_HEIGHT, 0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);

    xcb_icccm_set_wm_name(context.connection, context.window, XCB_ATOM_STRING,
            8, strlen("hello"), "hello");

    load_and_set_cursor(&context);

    xcb_map_window(context.connection, context.window);
    xcb_atom_t protocols[] =
    {
        context.wm_delete_window,
    };
    xcb_icccm_set_wm_protocols(context.connection, context.window,
            context.wm_protocols, 1, protocols);

    xcb_size_hints_t hints = {};
    xcb_icccm_size_hints_set_max_size(&hints, START_WIDTH, START_HEIGHT);
    xcb_icccm_size_hints_set_min_size(&hints, START_WIDTH, START_HEIGHT);
    xcb_icccm_set_wm_size_hints(context.connection, context.window,
            XCB_ICCCM_WM_STATE_NORMAL, &hints);

    hhxcb_offscreen_buffer buffer = {};
    hhxcb_resize_backbuffer(&context, &buffer, START_WIDTH, START_HEIGHT);

    xcb_flush(context.connection);

    hhxcb_sound_output sound_output = {};
    sound_output.samples_per_second = 48000;
    sound_output.bytes_per_sample = sizeof(int16) * 2;
    sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
    hhxcb_init_alsa(&context, &sound_output);

    int16 *sample_buffer = (int16 *)malloc(sound_output.secondary_buffer_size);

    thread_context t = {};

	hhxcb_thread_info ThreadInfo[15] = {};

	work_queue Queue = {};
	
	uint32 InitialCount = 0;

	// NOTE: "sem_init" has an error if it is passed "Queue.SemaphoreHandle"
	sem_t SemaphoreHandle = {};
	sem_init(&SemaphoreHandle, 0, InitialCount);
	Queue.SemaphoreHandle = &SemaphoreHandle;
	
	for(uint32 ThreadIndex = 0;
		ThreadIndex < ArrayCount(ThreadInfo);
		++ThreadIndex)
	{
		hhxcb_thread_info* Info = ThreadInfo + ThreadIndex;

		Info->Queue = &Queue;
		Info->LogicalThreadIndex = ThreadIndex;
		pthread_t thread = {};
		pthread_attr_t attr = {};
		pthread_attr_init(&attr);
		pthread_create(&thread, &attr, &ThreadFunction, Info);
	}

	PushString(&Queue, "String A0");
	PushString(&Queue, "String A1");
	PushString(&Queue, "String A2");
	PushString(&Queue, "String A3");
	PushString(&Queue, "String A4");
	PushString(&Queue, "String A5");
	PushString(&Queue, "String A6");
	PushString(&Queue, "String A7");
	PushString(&Queue, "String A8");
	PushString(&Queue, "String A9");
	
	PushString(&Queue, "String B0");
	PushString(&Queue, "String B1");
	PushString(&Queue, "String B2");
	PushString(&Queue, "String B3");
	PushString(&Queue, "String B4");
	PushString(&Queue, "String B5");
	PushString(&Queue, "String B6");
	PushString(&Queue, "String B7");
	PushString(&Queue, "String B8");
	PushString(&Queue, "String B9");

	work_queue_entry Entry = {};
    while(QueueWorkStillInProgress(&Queue))
	{
		Entry = CompleteAndGetNextWorkQueueEntry(&Queue, Entry);
		if(Entry.IsValid)
		{
			DoWorkerWork(Entry, 15);
		}
	}

    game_memory m = {};
    m.PermanentStorageSize = 256 * 1024 * 1024;
    m.TransientStorageSize = 256 * 1024 * 1024;
    state.total_size = m.PermanentStorageSize + m.TransientStorageSize;
    state.game_memory_block = calloc(state.total_size, sizeof(uint8));
    m.PermanentStorage = (uint8 *)state.game_memory_block;
    m.TransientStorage =
        (uint8_t *)m.PermanentStorage + m.TransientStorageSize;
#ifdef HANDMADE_INTERNAL
    m.DEBUGPlatformFreeFileMemory = debug_xcb_free_file_memory;
    m.DEBUGPlatformReadEntireFile = debug_xcb_read_entire_file;
    m.DEBUGPlatformWriteEntireFile = debug_xcb_write_entire_file;
#endif

    hhxcb_init_replays(&state);

    timespec last_counter = {};
    timespec flip_wall_clock = {}; // Actually monotonic clock
    clock_gettime(HHXCB_CLOCK, &last_counter);
    clock_gettime(HHXCB_CLOCK, &flip_wall_clock);

    game_input input[2] = {};
    game_input *new_input = &input[0];
    game_input *old_input = &input[1];

    int64_t next_controller_refresh = 0;

    while(!context.ending_flag)
    {
        if (last_counter.tv_sec >= next_controller_refresh)
        {
            hhxcb_refresh_controllers(&context);
            next_controller_refresh = last_counter.tv_sec + 1;
        }

        struct stat library_statbuf = {};
        stat(source_game_code_library_path, &library_statbuf);
        if (library_statbuf.st_mtime != game_code.library_mtime)
        {
            hhxcb_unload_game(&game_code);
            hhxcb_load_game(&game_code, source_game_code_library_path);
        }

        new_input->dtForFrame = target_nanoseconds_per_frame / (1024.0 * 1024 * 1024);

        hhxcb_process_events(&context, &state, new_input, old_input);

		// NOTE: setup game_buffer.Memory upside down and set
		// game_buffer.pitch negative, so the game would fill the
		// backbuffer upside down. XCB doesn't seem to have an
		// option to flip the image.
		
        game_offscreen_buffer game_buffer = {};
        game_buffer.Memory = ((uint8*)buffer.xcb_image->data)+
			(buffer.width*(buffer.height-1)*buffer.bytes_per_pixel);
        game_buffer.Width = buffer.width;
        game_buffer.Height = buffer.height;
        game_buffer.Pitch = -buffer.pitch;

        if (state.recording_index)
        {
            hhxcb_record_input(&state, new_input);
        }
        if (state.playback_index)
        {
            hhxcb_playback_input(&state, new_input);
        }
        if (game_code.UpdateAndRender)
        {
            game_code.UpdateAndRender(&t, &m, new_input, &game_buffer);
            HandleDebugCycleCounter(&m);
        }
	
        game_sound_output_buffer sound_buffer;
        sound_buffer.SamplesPerSecond = sound_output.samples_per_second;
        sound_buffer.SampleCount = sound_output.samples_per_second / 30;
        sound_buffer.Samples = sample_buffer;

        int err, frames;
        snd_pcm_sframes_t delay, avail;
        snd_pcm_avail_delay(context.handle, &avail, &delay);
        if (avail == sound_output.secondary_buffer_size)
        {
            // NOTE(nbm): Full available buffer, starting with ~60ms of silence
            bzero(sample_buffer, sound_buffer.SampleCount * sound_output.bytes_per_sample);
            snd_pcm_writei(context.handle, sample_buffer, sound_buffer.SampleCount);
            snd_pcm_writei(context.handle, sample_buffer, sound_buffer.SampleCount);
            snd_pcm_writei(context.handle, sample_buffer, sound_buffer.SampleCount);
            snd_pcm_writei(context.handle, sample_buffer, sound_buffer.SampleCount);
        }
        else
        {
            uint32 target_available_frames = sound_output.secondary_buffer_size;
            target_available_frames -= (sound_buffer.SampleCount * 1);
            if (avail - target_available_frames < sound_buffer.SampleCount)
            {
                sound_buffer.SampleCount += avail - target_available_frames;
            }
        }
        game_code.GetSoundSamples(&t, &m, &sound_buffer);
        if (sound_buffer.SampleCount > 0) {
            frames = snd_pcm_writei(context.handle, sample_buffer, sound_buffer.SampleCount);

            if (frames < 0)
            {
                frames = snd_pcm_recover(context.handle, frames, 0);
            }
            if (frames < 0) {
                printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                break;
            }
            if (frames > 0 && frames < sound_buffer.SampleCount)
            {
                printf("Short write (expected %i, wrote %i)\n", sound_buffer.SampleCount, frames);
            }
        }

        xcb_image_put(context.connection, buffer.xcb_pixmap_id,
                buffer.xcb_gcontext_id, buffer.xcb_image, 0, 0, 0);
        xcb_flush(context.connection);
		
        timespec target_counter = {};
        target_counter.tv_sec = last_counter.tv_sec;
        target_counter.tv_nsec = last_counter.tv_nsec + target_nanoseconds_per_frame;
        if (target_counter.tv_nsec > (1000 * 1000 * 1000))
        {
            target_counter.tv_sec++;
            target_counter.tv_nsec %= (1000 * 1000 * 1000);
        }

        timespec work_counter = {};
        clock_gettime(HHXCB_CLOCK, &work_counter);

        bool32 might_need_sleep = 0;
        if (work_counter.tv_sec < target_counter.tv_sec)
        {
            might_need_sleep = 1;
        }
        else if ((work_counter.tv_sec == target_counter.tv_sec) && (work_counter.tv_nsec < target_counter.tv_nsec))
        {
            might_need_sleep = 1;
        }
        if (might_need_sleep) {
            timespec sleep_counter = {};
            sleep_counter.tv_nsec = target_counter.tv_nsec - work_counter.tv_nsec;
            if (sleep_counter.tv_nsec < 0) {
                sleep_counter.tv_nsec += (1000 * 1000 * 1000);
            }
            // To closest ms
            sleep_counter.tv_nsec -= sleep_counter.tv_nsec % (1000 * 1000);
            if (sleep_counter.tv_nsec > 0) {
                timespec remaining_sleep_counter = {};
                nanosleep(&sleep_counter, &remaining_sleep_counter);
            }
            else
            {
                // TODO(nbm): Log missed sleep
            }
        }

        timespec spin_counter = {};
        clock_gettime(HHXCB_CLOCK, &spin_counter);
        while (spin_counter.tv_sec <= target_counter.tv_sec && spin_counter.tv_nsec < target_counter.tv_nsec) {
            clock_gettime(HHXCB_CLOCK, &spin_counter);
        }

        timespec end_counter = {};
        clock_gettime(HHXCB_CLOCK, &end_counter);

        long ns_per_frame = end_counter.tv_nsec - last_counter.tv_nsec;
        if (ns_per_frame < 0)
        {
            ns_per_frame += (1000 * 1000 * 1000) * (end_counter.tv_sec - last_counter.tv_sec);
        }
        last_counter = end_counter;
        real32 ms_per_frame = ns_per_frame / (1000 * 1000.0);

        xcb_copy_area(context.connection, buffer.xcb_pixmap_id, context.window,
                buffer.xcb_gcontext_id, 0,0, 0, 0, buffer.xcb_image->width,
                buffer.xcb_image->height);
        xcb_flush(context.connection);

        game_input *temp_input = new_input;
        new_input = old_input;
        old_input = temp_input;
    }

    snd_pcm_close(context.handle);

    // NOTE(nbm): Since auto-repeat seems to be a X-wide thing, let's be nice
    // and return it to where it was before?
    {
        uint32_t values[1] = {XCB_AUTO_REPEAT_MODE_DEFAULT};
        xcb_change_keyboard_control(context.connection, XCB_KB_AUTO_REPEAT_MODE, values);
    }

    xcb_flush(context.connection);
    xcb_disconnect(context.connection);
}
