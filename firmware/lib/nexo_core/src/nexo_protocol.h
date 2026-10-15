// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
#include <cstdint>

namespace nexo {

enum Opcode : uint8_t {
    OP_PING = 0x01,
    OP_GET_INFO = 0x02,
    OP_GET_DEVICE_INFO = 0x03,
    OP_SET_TIME = 0x04,
    OP_REBOOT = 0x05,
    OP_FACTORY_RESET = 0x06,
    OP_FIND_DEVICE = 0x07,
    OP_AUTH = 0x0F,
    OP_LIST_NOTES = 0x10,
    OP_GET_NOTE = 0x11,
    OP_PUT_NOTE = 0x12,
    OP_DELETE_NOTE = 0x13,
    OP_LIST_HOMEWORK = 0x20,
    OP_GET_HOMEWORK = 0x21,
    OP_PUT_HOMEWORK = 0x22,
    OP_DELETE_HOMEWORK = 0x23,
    OP_GET_SETTINGS = 0x30,
    OP_PUT_SETTINGS = 0x31,
    OP_SYNC_PULL = 0x40,
    OP_SYNC_PUSH = 0x41,
    OP_SYNC_SUMMARY = 0x42,
    OP_LIST_MUSIC = 0x50,
    OP_MUSIC_CONTROL = 0x51,
    OP_NOW_PLAYING = 0x52,
    OP_DELETE_MUSIC = 0x53,
};

constexpr uint8_t kResponseBit = 0x80;

enum Status : uint8_t {
    ST_OK = 0x00,
    ST_UNKNOWN = 0x01,
    ST_BAD_REQUEST = 0x02,
    ST_NOT_FOUND = 0x03,
    ST_UNAUTHORIZED = 0x04,
    ST_CONFLICT = 0x05,
    ST_TOO_LARGE = 0x06,
    ST_STORAGE = 0x07,
    ST_BUSY = 0x08,
    ST_VERSION = 0x09,
    ST_LOCKED = 0x0A,
};

}  // namespace nexo
