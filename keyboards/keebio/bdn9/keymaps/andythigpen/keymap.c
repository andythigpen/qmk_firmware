/* Copyright 2019 Danny Nguyen <danny@keeb.io>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H
#include "print.h"
#include "raw_hid.h"
#include "rgblight.h"
#include "rgblight_list.h"

// LEDs under the board
#define LED_UNDER_L 9
#define LED_UNDER_R 10

// layers
enum custom_layers {
    _DEFAULT_LAYER,
    _PROGRAMMING_LAYER,
    _DEBUGGING_LAYER,
    _SLACK_LAYER,
    _TEAMS_LAYER,
};

enum encoder_names {
  _LEFT,
  _RIGHT,
  _MIDDLE,
};

enum custom_keycodes {
    END_DEBUG = SAFE_RANGE,
    START_DEBUG,
    TEST_LAST,
    TEST_SUITE,
    TEST_FILE,
    TEST_NEAREST,
    RUN_SERVICE,
    MUTE_TEAMS,
    MUTE_SLACK,
    MUTE_STATUS,
    START_SLACK,
    START_TEAMS,
    END_CALL
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Default Fn layout */
    /*
       | Mute | Programming Layer | Slack |
       | | | |
       | | | |
     */
    [_DEFAULT_LAYER] = LAYOUT(
        KC_MUTE,  TO(_PROGRAMMING_LAYER),  START_SLACK,
        KC_F13,   KC_F14,  KC_F15,
        KC_F16,   KC_F17,  KC_F18
    ),
    /* Programming */
    /*
        | _          | Layer 0   | Debugging Layer |
        | Test Last  | Start dbg | Run Service  |
        | Test Suite | Test File | Test Nearest |
     */
    [_PROGRAMMING_LAYER] = LAYOUT(
        _______,    TO(_DEFAULT_LAYER), _______,
        TEST_LAST,  START_DEBUG,        RUN_SERVICE,
        TEST_SUITE, TEST_FILE,          TEST_NEAREST
    ),
    /* Debugging keys/macros */
    /*
        | _                 | Layer 0        | End debugging |
        | Toggle Breakpoint | Start/Continue | Stop      |
        | Step Out          | Step Over      | Step Into |
     */
    [_DEBUGGING_LAYER] = LAYOUT(
        _______, TO(_DEFAULT_LAYER),  END_DEBUG,
        KC_F9,   KC_F5,  KC_F3,
        KC_F12,  KC_F10, KC_F11
    ),
    /*
        | _         | Layer 0   | Teams layer |
        | Change mute status | Mute call | Mute call |
        | Mute call | Mute call | Mute call |
     */
    [_SLACK_LAYER] = LAYOUT(
        _______,       END_CALL,        START_TEAMS,
        MUTE_STATUS,    MUTE_SLACK,      MUTE_SLACK,
        MUTE_SLACK,    MUTE_SLACK,      MUTE_SLACK
    ),
    /*
        | _         | Layer 0   | Teams layer |
        | Change mute status | Mute call | Mute call |
        | Mute call | Mute call | Mute call |
     */
    [_TEAMS_LAYER] = LAYOUT(
        _______,       END_CALL,        START_SLACK,
        MUTE_STATUS,    MUTE_TEAMS,      MUTE_TEAMS,
        MUTE_TEAMS,    MUTE_TEAMS,      MUTE_TEAMS
    ),
};

bool muted_status = false;
void set_mute_status(bool muted);
void end_call(void);

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
    case END_DEBUG:
        if (record->event.pressed) {
            SEND_STRING(":VimspectorReset" SS_TAP(X_ENTER));
            layer_move(_PROGRAMMING_LAYER);
        }
        break;
    case START_DEBUG:
        if (record->event.pressed) {
            tap_code(KC_F5);
            layer_move(_DEBUGGING_LAYER);
        }
        break;
    case TEST_LAST:
        if (record->event.pressed)
            SEND_STRING(":TestLast" SS_TAP(X_ENTER));
        break;
    case TEST_SUITE:
        if (record->event.pressed)
            SEND_STRING(":TestSuite" SS_TAP(X_ENTER));
        break;
    case TEST_FILE:
        if (record->event.pressed)
            SEND_STRING(":TestFile" SS_TAP(X_ENTER));
        break;
    case TEST_NEAREST:
        if (record->event.pressed)
            SEND_STRING(":TestNearest" SS_TAP(X_ENTER));
        break;
    case MUTE_TEAMS:
        if (record->event.pressed) {
            SEND_STRING(SS_LCMD(SS_LSFT("m")));
            set_mute_status(!muted_status);
        }
        break;
    case MUTE_SLACK:
        if (record->event.pressed) {
            SEND_STRING("m");
            set_mute_status(!muted_status);
        }
        break;
    case START_SLACK:
        if (record->event.pressed) {
            set_mute_status(false);
            layer_move(_SLACK_LAYER);
        }
        break;
    case START_TEAMS:
        if (record->event.pressed) {
            set_mute_status(true);
            layer_move(_TEAMS_LAYER);
        }
        break;
    case END_CALL:
        if (record->event.pressed) {
            end_call();
            layer_clear();
        }
        return false; // do not continue to process keys after moving layers
    case MUTE_STATUS:
        if (record->event.pressed)
            set_mute_status(!muted_status);
        break;
    }
    return true;
};

void keyboard_post_init_user(void) {
  debug_enable = true;
  // debug_matrix = true;
  // debug_keyboard=true;
  // debug_mouse=true;
}

void encoder_update_user(uint8_t index, bool clockwise) {
    /* dprintln("encoder_update_user"); */
    if (index == _LEFT) {
        if (clockwise) {
            tap_code(KC_VOLU);
        } else {
            tap_code(KC_VOLD);
        }
    }
}

/* void raw_hid_receive(uint8_t *data, uint8_t length) { */
/*     #<{(| dprintf("raw_hid_receive len=%d\n", length); |)}># #<{(| for (uint8_t i = 0; i < length; ++i) { |)}># */
/*     #<{(|     dprintf("%x ", data[i]); |)}># */
/*     #<{(| } |)}># */
/*     #<{(| dprintf("\n"); |)}># */
/*     #<{(| uint8_t mode = rgblight_get_mode(); |)}># */
/*     #<{(| raw_hid_send(&mode, sizeof(mode)); |)}># */
/*     #<{(| rgblight_setrgb_at(255, 0, 0, 2); |)}># */
/*     #<{(| rgblight_setrgb_at(0, 255, 0, 3); |)}># */
/*     #<{(| rgblight_setrgb_at(0, 0, 255, 4); |)}># */
/*  */
/*     #<{(| if (length == 1 && data[0] == 0xAB) { |)}># */
/*     #<{(|     rgblight_sethsv_noeeprom_red(); |)}># */
/*     #<{(| } |)}># */
/*  */
/*     #<{(| uint8_t *command_id   = &(data[0]); |)}># */
/*     #<{(| uint8_t *command_data = &(data[1]); |)}># */
/*     #<{(|  |)}># */
/*     #<{(| switch (*command_id) { |)}># */
/*     #<{(|     case 0x03: { |)}># */
/*     #<{(|         switch (command_data[0]) { |)}># */
/*     #<{(|             case 0xFF: |)}># */
/*     #<{(|                 rgblight_sethsv_red(); |)}># */
/*     #<{(|                 break; |)}># */
/*     #<{(|         } |)}># */
/*     #<{(|         break; |)}># */
/*     #<{(|     } |)}># */
/*     #<{(| } |)}># */
/* } */

#define STATE_CMD               0
#define STATE_LEN               1
#define STATE_BUF               2

#define MAX_BUF_LEN             4

#define CMD_RESET               1   // reset the keyboard to flash
#define CMD_SET_MODE            2   // set mode
#define CMD_TOGGLE_MATRIX       3   // toggle RGB matrix on/off
#define CMD_SET_MATRIX_HSV      4   // set all HSV matrix values
#define CMD_TOGGLE_INDICATOR    5   // toggle indicator for specific key LED
#define CMD_SET_INDICATOR_HSV   6   // set HSV values for specific key LED
#define CMD_ENABLE_INDICATOR    7   // enable indicator for specific key LED
#define CMD_DISABLE_INDICATOR   8   // disable indicator for specific key LED
#define CMD_ACTIVATE_LAYER      9   // activates a specific layer and deactivates all others (except default)
#define CMD_SET_SPEED           10  // sets animation speed
#define CMD_SET_MUTE_STATUS     11  // sets mute status
#define CMD_END_CALL            12  // clears mute status and resets key LEDs

uint8_t cmd, len, buf_idx;
uint8_t buf[MAX_BUF_LEN];
uint8_t state = STATE_CMD;
HSV indicator_hsv[RGBLED_NUM];
bool indicator_enable[RGBLED_NUM];

void process_cmd(void) {
    switch (cmd) {
        case CMD_SET_MODE:
            // { persist, mode }
            dprintf("set mode:%d persist:%d\n", buf[1], buf[0]);
            if (buf[0])
                rgb_matrix_mode(buf[1]);
            else
                rgb_matrix_mode_noeeprom(buf[1]);
            break;

        case CMD_TOGGLE_MATRIX:
            // { persist }
            dprintf("toggle matrix persist:%d\n", buf[0]);
            if (buf[0])
                rgb_matrix_toggle();
            else
                rgb_matrix_toggle_noeeprom();
            break;

        case CMD_SET_MATRIX_HSV:
            // { persist, h, s, v }
            dprintf("set matrix h:%d s:%d v:%d persist:%d\n", buf[1], buf[2], buf[3], buf[0]);
            if (buf[0])
                rgb_matrix_sethsv(buf[1], buf[2], buf[3]);
            else
                rgb_matrix_sethsv_noeeprom(buf[1], buf[2], buf[3]);
            break;

        case CMD_TOGGLE_INDICATOR:
            // { index }
            dprintf("toggle led:%d\n", buf[0]);
            if (buf[0] < RGBLED_NUM) {
                indicator_enable[buf[0]] = !indicator_enable[buf[0]];
            }
            break;

        case CMD_ENABLE_INDICATOR:
            // { index }
            dprintf("enable led:%d\n", buf[0]);
            if (buf[0] < RGBLED_NUM) {
                indicator_enable[buf[0]] = true;
            }
            break;

        case CMD_DISABLE_INDICATOR:
            // { index }
            dprintf("disable led:%d\n", buf[0]);
            if (buf[0] < RGBLED_NUM) {
                indicator_enable[buf[0]] = false;
            }
            break;

        case CMD_SET_INDICATOR_HSV: {
            // { index, h, s, v }
            dprintf("set indicator led:%d h:%d s:%d v:%d\n", buf[0], buf[1], buf[2], buf[3]);
            if (buf[0] < RGBLED_NUM) {
                HSV *hsv = &indicator_hsv[buf[0]];
                hsv->h = buf[1];
                hsv->s = buf[2];
                hsv->v = buf[3];
            }
            break;
        }

        case CMD_RESET:
            // check for a magic key to make sure
            if (buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xF0 && buf[3] == 0x00) {
                reset_keyboard();
            }
            break;

        case CMD_ACTIVATE_LAYER:
            // { layer }
            dprintf("activate layer:%d\n", buf[0]);
            layer_move(buf[0]);
            break;

        case CMD_SET_SPEED:
            // { persist, speed }
            dprintf("set speed:%d persist:%d\n", buf[1], buf[0]);
            if (buf[0])
                rgb_matrix_set_speed(buf[1]);
            else
                rgb_matrix_set_speed_noeeprom(buf[1]);
            break;

        case CMD_SET_MUTE_STATUS:
            // { enabled }
            dprintf("set mute:%d\n", buf[0]);
            set_mute_status(buf[0]);
            break;

        case CMD_END_CALL:
            dprintf("end call\n");
            end_call();
            layer_move(_DEFAULT_LAYER);
            break;
    }
}

void virtser_recv(uint8_t serIn) {
    dprintf("virtser_recv (state:%x) %x\n", state, serIn);

    switch (state) {
        case STATE_CMD:
            cmd = serIn;
            len = buf_idx = 0;
            state = STATE_LEN;
            break;

        case STATE_LEN:
            len = serIn;
            if (len == 0) {
                process_cmd();
                state = STATE_CMD;
            }
            else
                state = STATE_BUF;
            break;

        case STATE_BUF:
            if (buf_idx < MAX_BUF_LEN)
                buf[buf_idx] = serIn;

            buf_idx += 1;
            if (buf_idx == len) {
                process_cmd();
                state = STATE_CMD;
            }
            break;

        default:
            state = STATE_CMD;
            break;
    }
}

void rgb_matrix_indicators_kb(void) {
    for (uint8_t i = 0; i < RGBLED_NUM; ++i) {
        if (!indicator_enable[i])
            continue;

        HSV *hsv = &indicator_hsv[i];
        RGB rgb = hsv_to_rgb(*hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
}

void set_led_under_hsv(uint8_t h, uint8_t s, uint8_t v) {
    indicator_hsv[LED_UNDER_L].h = h;
    indicator_hsv[LED_UNDER_L].s = s;
    indicator_hsv[LED_UNDER_L].v = v;
    indicator_hsv[LED_UNDER_R] = indicator_hsv[LED_UNDER_L];
    indicator_enable[LED_UNDER_L] = indicator_enable[LED_UNDER_R] = true;
}

void set_mute_status(bool muted) {
    muted_status = muted;

    if (muted) {
        rgb_matrix_sethsv_noeeprom(85, 244, 150);
        rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    }
    else {
        rgb_matrix_sethsv_noeeprom(255, 255, 255);
        rgb_matrix_mode_noeeprom(RGB_MATRIX_BREATHING);
    }
}

void end_call(void) {
    rgb_matrix_sethsv_noeeprom(0, 0, 20);
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    muted_status = false;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    dprintf("layer_state %x\n", state);
    switch (get_highest_layer(state)) {
        case _DEFAULT_LAYER:
            set_led_under_hsv(0, 0, 200);
            break;
        case _PROGRAMMING_LAYER:
            set_led_under_hsv(150, 250, 255);
            break;
        case _DEBUGGING_LAYER:
            set_led_under_hsv(10, 250, 255);
            break;
        case _SLACK_LAYER:
            set_led_under_hsv(30, 250, 255);
            break;
        case _TEAMS_LAYER:
            set_led_under_hsv(180, 250, 255);
            break;
    }
    return state;
}
