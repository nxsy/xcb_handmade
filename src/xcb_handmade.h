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

#define HHXCB_STATE_FILE_NAME_LENGTH (1024)
#define HHXCB_CLOCK CLOCK_MONOTONIC
#define HHXCB_MAX_CONTROLLERS 4
#define HHXCB_NUM_REPLAYS 4

struct hhxcb_replay_buffer
{
    uint32 file_handle;
    uint32 memory_map;
    char filename[HHXCB_STATE_FILE_NAME_LENGTH];
    void *memory_block;
};

struct hhxcb_state {
    uint64_t total_size;
    void *game_memory_block;
    hhxcb_replay_buffer replay_buffers[HHXCB_NUM_REPLAYS];

    uint32_t recording_fd;
    uint32_t recording_index;

    uint32_t playback_fd;
    uint32_t playback_index;

    char binary_name[HHXCB_STATE_FILE_NAME_LENGTH];
    char *one_past_binary_filename_slash;
};

struct hhxcb_game_code
{
    void *library_handle;
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

	debug_game_frame_end *DEBUGFrameEnd;
	
    bool32 is_valid;
    time_t library_mtime;
};

struct hhxcb_offscreen_buffer
{
    xcb_image_t *xcb_image;
    xcb_pixmap_t xcb_pixmap_id;
    xcb_gcontext_t xcb_gcontext_id;

    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct hhxcb_controller_info {
    bool32 is_active;
    char path[HHXCB_STATE_FILE_NAME_LENGTH]; // /dev/input filename of the controller
    int fd;
    int button_ioctl_to_use;
    uint16 axis_dead_zones[ABS_MAX+1];
    bool axis_inverted[ABS_MAX+1];
    int left_thumb_x_axis;
    int left_thumb_y_axis;
    int right_thumb_x_axis;
    int right_thumb_y_axis;
    int dpad_x_axis;
    int dpad_y_axis;
    int a_button;
    int b_button;
    int x_button;
    int y_button;
    int l1_button;
    int r1_button;
    int back_button;
    int start_button;
};

enum modifiers
{
	MOD_SHIFT,
	MOD_ALT,
	MOD_CONTROL,
};

struct hhxcb_context
{
    bool32 ending_flag;

    const xcb_setup_t *setup;
    xcb_format_t *fmt;
    xcb_connection_t *connection;
    xcb_key_symbols_t *key_symbols;
    xcb_window_t window;
	Display *display;

    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;

    xcb_pixmap_t cursor_pixmap_id;
    xcb_cursor_t cursor_id;

    bool32 need_controller_refresh;
    hhxcb_controller_info controller_info[HHXCB_MAX_CONTROLLERS];

	game_button_state modifier_keys[3];
	
    snd_pcm_t *alsa_handle;
    snd_output_t *alsa_log;
};

struct hhxcb_sound_output
{
    int samples_per_second;
    uint32 running_sample_index;
    int bytes_per_sample;
    uint32 sound_buffer_size;
	uint32 safety_samples;
	u32 channels;
	u64 period_size;
	u32 sample_count;
};
