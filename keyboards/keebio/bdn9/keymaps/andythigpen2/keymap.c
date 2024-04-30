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
#include <stdint.h>
#include QMK_KEYBOARD_H

#include "raw_hid.h"
#include "tmk_core/protocol/usb_descriptor.h"

// TODO: Fix KVM key default indicator color

// clang-format off
// LEDs under the board
#define LED_UNDER_L 9
#define LED_UNDER_R 10

// Raw HID protocol commands
#define CMD_RESET               1   // reset the keyboard to flash
#define CMD_SET_MODE            2   // set mode
#define CMD_TOGGLE_MATRIX       3   // toggle RGB matrix on/off
#define CMD_SET_MATRIX_HSV      4   // set all HSV matrix values
#define CMD_TOGGLE_INDICATOR    5   // toggle indicator for specific key LED
#define CMD_SET_INDICATOR_RGB   6   // set RGB values for specific key LED
#define CMD_ENABLE_INDICATOR    7   // enable indicator for specific key LED
#define CMD_DISABLE_INDICATOR   8   // disable indicator for specific key LED
#define CMD_ACTIVATE_LAYER      9   // activates a specific layer and deactivates all others (except default)
#define CMD_SET_SPEED           10  // sets animation speed
#define CMD_SET_MUTE_STATUS     11  // sets mute status
#define CMD_END_CALL            12  // clears mute status and resets key LEDs
#define CMD_ECHO                13  // for testing, just echos the received bytes back
#define CMD_CONNECT             14  // sent by the host program after connecting
#define CMD_DISCONNECT          15  // sent by the host program before disconnecting

// Raw HID events for keyboard host daemon
#define EVENT_MUTE_SLACK      2
#define EVENT_MUTE_TEAMS      3
#define EVENT_FOCUS_SLACK     4
#define EVENT_FOCUS_TEAMS     5
#define EVENT_START_SLACK     6
#define EVENT_START_TEAMS     7
#define EVENT_END_CALL        8

// layers
enum custom_layers {
    _DEFAULT_LAYER,
    _TEAMS_LAYER,
    _SLACK_LAYER,
    _MAX_LAYER,
};

enum custom_keycodes {
    END_DEBUG = SAFE_RANGE,
    MUTE_TEAMS,
    MUTE_TEAMS_FOCUS,
    FOCUS_TEAMS,
    MUTE_SLACK,
    MUTE_SLACK_FOCUS,
    FOCUS_SLACK,
    MUTE_STATUS,
    START_SLACK,
    START_TEAMS,
    END_CALL,
    CUSTOM_MACRO1,      // cmd+F13
    CUSTOM_MACRO2,      // cmd+F14
    CUSTOM_MACRO3,      // cmd+F15
    CUSTOM_MACRO4,      // cmd+F16
    CUSTOM_MACRO5,      // cmd+F17
    CUSTOM_MACRO6,      // cmd+F18
    KVM,
};

enum encoder_names {
    _LEFT,
    _RIGHT,
    _MIDDLE,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Default Fn layout */
    /*
       | Mute | Programming Layer | Slack |
       | | | |
       | | | |
     */
    [_DEFAULT_LAYER] = LAYOUT(
        KC_MUTE,       START_SLACK,   START_TEAMS,
        CUSTOM_MACRO1, CUSTOM_MACRO2, CUSTOM_MACRO3,
        CUSTOM_MACRO4, CUSTOM_MACRO5, KVM
    ),
    /*
        | _         | Layer 0   | Teams layer |
        | Change mute status | Mute call | Mute call |
        | Focus app | Mute call | Mute call |
     */
    [_TEAMS_LAYER] = LAYOUT(
        _______,        END_CALL,           START_SLACK,
        MUTE_STATUS,    MUTE_TEAMS,         MUTE_TEAMS,
        FOCUS_TEAMS,    MUTE_TEAMS_FOCUS,   MUTE_TEAMS_FOCUS
    ),
    /*
        | _         | Layer 0   | Teams layer |
        | Change mute status | Mute call | Mute call |
        | Focus app | Mute call | Mute call |
     */
    [_SLACK_LAYER] = LAYOUT(
        _______,        END_CALL,           START_TEAMS,
        MUTE_STATUS,    MUTE_SLACK,         MUTE_SLACK,
        FOCUS_SLACK,    MUTE_SLACK_FOCUS,   MUTE_SLACK_FOCUS
    ),
};
// clang-format on

bool connected = false;
bool muted_status = false;
bool kvm_toggle = false;
// key indicators
#define INDICATOR_LAYERS _MAX_LAYER
RGB indicator_rgb[INDICATOR_LAYERS][RGBLIGHT_LED_COUNT];
bool indicator_enable[INDICATOR_LAYERS][RGBLIGHT_LED_COUNT];

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == _LEFT) {
        if (clockwise) {
            tap_code(KC_VOLU);
        } else {
            tap_code(KC_VOLD);
        }
    }
    return false;
}

bool rgb_matrix_indicators_user(void) {
    uint32_t layer = get_highest_layer(layer_state);
    if (layer >= INDICATOR_LAYERS)
        return false;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; ++i) {
        if (!indicator_enable[layer][i])
            continue;

        RGB *rgb = &indicator_rgb[layer][i];
        rgb_matrix_set_color(i, rgb->r, rgb->g, rgb->b);
    }
    return false;
}

void set_indicator_rgb(uint8_t layer, uint8_t key, uint8_t r, uint8_t g, uint8_t b) {
    RGB *rgb = &indicator_rgb[layer][key];
    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}

void set_led_under_rgb(uint8_t layer, uint8_t r, uint8_t g, uint8_t b) {
    set_indicator_rgb(layer, LED_UNDER_L, r, g, b);
    set_indicator_rgb(layer, LED_UNDER_R, r, g, b);
    indicator_enable[layer][LED_UNDER_L] = indicator_enable[layer][LED_UNDER_R] = true;
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

void send_event(uint8_t event) {
    if (!connected)
        return;

    // all messages must be RAW_EPSIZE
    // @see https://docs.qmk.fm/#/feature_rawhid?id=raw-hid
    uint8_t message[RAW_EPSIZE];
    memset(message, 0, sizeof(message));
    message[0] = event;
    raw_hid_send(message, sizeof(message));
}

void set_default_colors(void) {
    rgb_matrix_sethsv_noeeprom(0, 0, 20);
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    set_led_under_rgb(_DEFAULT_LAYER, 100, 100, 100);
    // KVM key
    set_indicator_rgb(_DEFAULT_LAYER, 8, 20, 10, 20);
    indicator_enable[_DEFAULT_LAYER][8] = true;
}

void end_call(void) {
    set_default_colors();
    muted_status = false;
    send_event(EVENT_END_CALL);
}

void echo(uint8_t *data, uint8_t length) {
    uint8_t response[length];
    memset(response, 0, length);
    memcpy(response, data, length);
    raw_hid_send(response, length);
}

layer_state_t layer_state_set_user(layer_state_t state) {
    uint32_t layer = get_highest_layer(state);
    switch (layer) {
        case _DEFAULT_LAYER: {
            set_default_colors();
            break;
        }
        case _TEAMS_LAYER:
            set_led_under_rgb(layer, 150, 25, 255);
            break;
        case _SLACK_LAYER:
            set_led_under_rgb(layer, 255, 250, 25);
            break;
    }
    return state;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
    /** Teams **/
    case START_TEAMS:
        if (record->event.pressed) {
            set_mute_status(true);
            layer_move(_TEAMS_LAYER);
            send_event(EVENT_START_TEAMS);
        }
        break;
    case MUTE_TEAMS:
        if (record->event.pressed) {
            SEND_STRING(SS_LCMD(SS_LSFT("m")));
            set_mute_status(!muted_status);
        }
        break;
    case MUTE_TEAMS_FOCUS:
        if (record->event.pressed) {
            send_event(EVENT_MUTE_TEAMS);
            set_mute_status(!muted_status);
        }
        break;
    case FOCUS_TEAMS:
        if (record->event.pressed) {
            send_event(EVENT_FOCUS_TEAMS);
        }
        break;

    /** Slack **/
    case START_SLACK:
        if (record->event.pressed) {
            set_mute_status(false);
            layer_move(_SLACK_LAYER);
            send_event(EVENT_START_SLACK);
        }
        break;
    case MUTE_SLACK:
        if (record->event.pressed) {
            SEND_STRING(SS_LCMD(SS_LSFT(SS_TAP(X_SPC))));
            set_mute_status(!muted_status);
        }
        break;
    case MUTE_SLACK_FOCUS:
        if (record->event.pressed) {
            send_event(EVENT_MUTE_SLACK);
            set_mute_status(!muted_status);
        }
        break;
    case FOCUS_SLACK:
        if (record->event.pressed) {
            send_event(EVENT_FOCUS_SLACK);
        }
        break;

    /** Common/other **/
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
    case CUSTOM_MACRO1:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F13)));
        break;
    case CUSTOM_MACRO2:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F14)));
        break;
    case CUSTOM_MACRO3:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F15)));
        break;
    case CUSTOM_MACRO4:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F16)));
        break;
    case CUSTOM_MACRO5:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F17)));
        break;
    case CUSTOM_MACRO6:
        if (record->event.pressed)
            SEND_STRING(SS_LCMD(SS_TAP(X_F18)));
        break;
    case KVM:
        if (record->event.pressed) {
            tap_code_delay(KC_LCTL, 10);
            tap_code_delay(KC_LCTL, 10);
            if (kvm_toggle)
                tap_code(KC_1);
            else
                tap_code(KC_2);
            kvm_toggle = !kvm_toggle;
        }
        return false;
    }
    return true;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t  cmd = data[0];
    uint8_t *buf = &(data[1]);
    switch (cmd) {
        case CMD_RESET:
            // check for a magic key to make sure
            if (buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xF0 && buf[3] == 0x00) {
                reset_keyboard();
            }
            break;

        case CMD_SET_MODE:
            // { persist, mode }
            if (buf[0])
                rgb_matrix_mode(buf[1]);
            else
                rgb_matrix_mode_noeeprom(buf[1]);
            break;

        case CMD_TOGGLE_MATRIX:
            // { persist }
            if (buf[0])
                rgb_matrix_toggle();
            else
                rgb_matrix_toggle_noeeprom();
            break;

        case CMD_SET_MATRIX_HSV:
            // { persist, h, s, v }
            if (buf[0])
                rgb_matrix_sethsv(buf[1], buf[2], buf[3]);
            else
                rgb_matrix_sethsv_noeeprom(buf[1], buf[2], buf[3]);
            break;

        case CMD_TOGGLE_INDICATOR:
            // { layer, index }
            if (buf[0] < INDICATOR_LAYERS && buf[1] < RGBLIGHT_LED_COUNT) {
                indicator_enable[buf[0]][buf[1]] = !indicator_enable[buf[0]][buf[1]];
            }
            break;

        case CMD_ENABLE_INDICATOR:
            // { layer, index }
            if (buf[0] < INDICATOR_LAYERS && buf[1] < RGBLIGHT_LED_COUNT) {
                indicator_enable[buf[0]][buf[1]] = true;
            }
            break;

        case CMD_DISABLE_INDICATOR:
            // { layer, index }
            if (buf[0] < INDICATOR_LAYERS && buf[1] < RGBLIGHT_LED_COUNT) {
                indicator_enable[buf[0]][buf[1]] = false;
            }
            break;

        case CMD_SET_INDICATOR_RGB: {
            // { layer, index, r, g, b }
            if (buf[0] < INDICATOR_LAYERS && buf[1] < RGBLIGHT_LED_COUNT) {
                set_indicator_rgb(buf[0], buf[1], buf[2], buf[3], buf[4]);
            }
            break;
        }

        case CMD_ACTIVATE_LAYER:
            // { layer }
            layer_move(buf[0]);
            break;

        case CMD_SET_SPEED:
            // { persist, speed }
            if (buf[0])
                rgb_matrix_set_speed(buf[1]);
            else
                rgb_matrix_set_speed_noeeprom(buf[1]);
            break;

        case CMD_SET_MUTE_STATUS:
            // { enabled }
            set_mute_status(buf[0]);
            break;

        case CMD_END_CALL:
            end_call();
            layer_move(_DEFAULT_LAYER);
            break;

        case CMD_ECHO:
            echo(data, length);
            break;

        case CMD_CONNECT:
            connected = true;
            break;

        case CMD_DISCONNECT:
            connected = false;
            break;
    }
}

void keyboard_post_init_user(void) {
    set_default_colors();
}
