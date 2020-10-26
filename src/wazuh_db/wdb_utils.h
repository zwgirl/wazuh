/*
 * Wazuh SQLite integration
 * Copyright (C) 2015-2020, Wazuh Inc.
 * October 23, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef WDB_UTILS_H
#define WDB_UTILS_H

void wdb_create_error_response(char** buffer, const char* format, ...);
void wdb_create_invalid_query_sintax_response(char** buffer, const char* query);

#endif
