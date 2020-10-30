/*
 * Wazuh Database Daemon
 * Copyright (C) 2015-2020, Wazuh Inc.
 * October 23, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <stdarg.h>
#include "wdb_utils.h"
#include "wdb.h"

void wdb_create_response(char** buffer, const char* format, ...) {
    if (!buffer) {
        mdebug2("No response created. Invalid pointer");
        return;
    }
    if(*buffer) {
        mdebug2("No response created. Possible memory leak");
        return;
    }

    os_calloc(WDB_MAX_ERR_SIZE, sizeof(char), *buffer);
    va_list args;
    va_start (args, format);
    vsnprintf (*buffer, WDB_MAX_ERR_SIZE, format, args);
    va_end (args);
}
