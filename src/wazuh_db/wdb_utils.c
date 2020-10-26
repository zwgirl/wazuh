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

void wdb_create_error_response(char** buffer, const char* format, ...) {
    os_calloc(WDB_MAX_ERR_SIZE, sizeof(char), *buffer);
    va_list args;
    va_start (args, format);
    vsnprintf (*buffer,256,format, args);
    va_end (args);
}

void wdb_create_invalid_query_sintax_response(char** buffer, const char* query) {
    wdb_create_error_response (buffer, "Invalid DB query syntax, near '%.32s'",query);
}

void wdb_create_invalid_syscheck_query_sintax_response(char** buffer, const char* query) {
    wdb_create_error_response (buffer, "Invalid Syscheck query syntax, near '%.32s'",query);
}

void wdb_create_cannot_execute_query_response(char** buffer, const char* db, const char* sqlite_err) {
    wdb_create_error_response (buffer, "Cannot execute %s database query; %s", db, sqlite_err);
}

void wdb_create_cannot_execute_SQL_query_response(char** buffer, const char* db, const char* sqlite_err) {
    wdb_create_error_response (buffer, "Cannot execute %s database query; %s", db, sqlite_err);
}

void wdb_create_invalid_SCA_response(char** buffer, const char* db, const char* sqlite_err) {
    wdb_create_error_response (buffer, "Cannot execute %s database query; %s", db, sqlite_err);
}
