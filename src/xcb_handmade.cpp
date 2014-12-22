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

#include <sys/stat.h> // stat, fstat

#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <fcntl.h>    // open, O_RDONLY
#include <stdio.h>    // printf
#include <stdlib.h>   // malloc, free
#include <string.h>   // strlen (I'm so lazy)
#include <time.h>     // clock_gettime, CLOCK_MONOTONIC
#include <unistd.h>   // readlink

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include "handmade.h"
#include "xcb_handmade.h"

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
    // TODO(nbm): Maybe finish writing this?

    debug_read_file_result result = {};
    uint32_t fd = open(Filename, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open file %s\n", Filename);
        return result;
    }

    struct stat statbuf = {};
    uint32_t stat_result = fstat(fd, &statbuf);

    if (stat_result != 0)
    {
        printf("Failed to stat opened file %s\n", Filename);
        return result;
    }
    printf("Reading file %s, stat_result is %u\n", Filename, stat_result);

    return result;
}
#endif

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
hhxcb_process_keyboard_message(game_button_state *new_state, bool32 is_down)
{
    if (new_state->EndedDown != is_down)
    {
        new_state->EndedDown = is_down;
        ++new_state->HalfTransitionCount;
    }
}

internal void
hhxcb_process_events(hhxcb_context *context, game_input *new_input, game_input *old_input)
{
    game_controller_input *old_keyboard_controller = GetController(old_input, 0);
    game_controller_input *new_keyboard_controller = GetController(new_input, 0);
    *new_keyboard_controller = {};
    new_keyboard_controller->IsConnected = true;
    for (
            int button_index = 0;
            button_index < ArrayCount(new_keyboard_controller->Buttons);
            ++button_index)
    {
        new_keyboard_controller->Buttons[button_index].EndedDown =
            old_keyboard_controller->Buttons[button_index].EndedDown;
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
                    hhxcb_process_keyboard_message(&new_keyboard_controller->Start, is_down);
                }
                else if (keysym == XK_space)
                {
                    hhxcb_process_keyboard_message(&new_keyboard_controller->Back, is_down);
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
                printf("xcb_client_message received, type %u\n", client_message_event->type);
                printf("data32[0] is %u\n", client_message_event->data.data32[0]);
                break;
            }
            default:
            {
                printf("Unhandled unknown response type: %u\n", event->response_type);
                break;
            }
        }
        free(event);
    };
}

int
main()
{
    hhxcb_state state = {};
    hhxcb_get_binary_name(&state);

    char source_game_code_library_path[HHXCB_STATE_FILE_NAME_LENGTH];
    hhxcb_build_full_filename(&state, (char *)"libhandmade.so",
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

#define START_WIDTH 1280
#define START_HEIGHT 720

    context.window = xcb_generate_id(context.connection);
    xcb_create_window(context.connection, XCB_COPY_FROM_PARENT, context.window,
            screen->root, 0, 0, START_WIDTH, START_HEIGHT, 10,
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

    thread_context t = {};

#ifdef HANDMADE_INTERNAL
    debug_xcb_read_entire_file(&t, (char *)"handmade.h");
#endif

    game_memory m = {};
    m.PermanentStorageSize = Megabytes(64);
    m.TransientStorageSize = Megabytes(64);
    state.total_size = m.PermanentStorageSize + m.TransientStorageSize;
    m.PermanentStorage = malloc(state.total_size);
    m.TransientStorage =
        (uint8_t *)m.PermanentStorage + m.TransientStorageSize;
    m.IsInitialized = 1;
#ifdef HANDMADE_INTERNAL
    m.DEBUGPlatformFreeFileMemory = debug_xcb_free_file_memory;
    m.DEBUGPlatformReadEntireFile;
    m.DEBUGPlatformWriteEntireFile;
#endif

    bool ending = 0;
    timespec last_counter = {};
    timespec flip_wall_clock = {}; // Actually monotonic clock
    clock_gettime(CLOCK_MONOTONIC, &last_counter);
    clock_gettime(CLOCK_MONOTONIC, &flip_wall_clock);

    game_input input[2] = {};
    game_input *new_input = &input[0];
    game_input *old_input = &input[1];

    while(!ending)
    {
        struct stat library_statbuf = {};
        stat(source_game_code_library_path, &library_statbuf);
        if (library_statbuf.st_mtime != game_code.library_mtime)
        {
            hhxcb_unload_game(&game_code);
            hhxcb_load_game(&game_code, source_game_code_library_path);
        }

        hhxcb_process_events(&context, new_input, old_input);

        if (context.ending_flag)
        {
            break;
        }

        game_offscreen_buffer game_buffer = {};
        game_buffer.Memory = buffer.xcb_image->data;
        game_buffer.Width = buffer.width;
        game_buffer.Height = buffer.height;
        game_buffer.Pitch = buffer.pitch;
        game_buffer.BytesPerPixel = buffer.bytes_per_pixel;

        game_code.UpdateAndRender(&t, &m, new_input, &game_buffer);

        xcb_image_put(context.connection, buffer.xcb_pixmap_id,
                buffer.xcb_gcontext_id, buffer.xcb_image, 0, 0, 0);

        xcb_copy_area(context.connection, buffer.xcb_pixmap_id, context.window,
                buffer.xcb_gcontext_id, 0,0, 0, 0, buffer.xcb_image->width,
                buffer.xcb_image->height);
        xcb_flush(context.connection);

        game_input *temp_input = new_input;
        new_input = old_input;
        old_input = temp_input;
    }

    // NOTE(nbm): Since auto-repeat seems to be a X-wide thing, let's be nice
    // and return it to where it was before?
    {
        uint32_t values[1] = {XCB_AUTO_REPEAT_MODE_DEFAULT};
        xcb_change_keyboard_control(context.connection, XCB_KB_AUTO_REPEAT_MODE, values);
    }

    xcb_flush(context.connection);
    xcb_disconnect(context.connection);
}
