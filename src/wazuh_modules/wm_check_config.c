/*
 * Wazuh Module - Configuration files checker
 * Copyright (C) 2015-2019, Wazuh Inc.
 * September, 2019
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */


#ifndef CLIENT
#if defined (__linux__) || defined (__MACH__)


#include "wm_check_config.h"
#define WARN_RESULT     "test was successful, however few errors have been found, please inspect your configuration file."


static void *wm_chk_conf_main();
static void wm_chk_conf_destroy();
cJSON *wm_chk_conf_dump(void);

const wm_context WM_CHK_CONF_CONTEXT = {
    "check_configuration",
    (wm_routine)wm_chk_conf_main,
    (wm_routine)wm_chk_conf_destroy,
    (cJSON * (*)(const void *))wm_chk_conf_dump
};


void *wm_chk_conf_main() {

    int sock, peer;
    char *buffer = NULL;
    ssize_t length;
    fd_set fdset;

    /* variables */
    char *filetype = NULL;
    char *filepath = NULL;

    if (sock = OS_BindUnixDomain(CHK_CONF_SOCK, SOCK_STREAM, OS_MAXSTR), sock < 0) {
        mterror(WM_CHECK_CONFIG_LOGTAG, "Unable to bind to socket '%s': (%d) %s.", CHK_CONF_SOCK, errno, strerror(errno));
        return NULL;
    }

    mtinfo(WM_CHECK_CONFIG_LOGTAG, "Starting configuration checker thread.");
    while(1) {
        // Wait for socket
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);

        switch (select(sock + 1, &fdset, NULL, NULL, NULL)) {
            case -1:
                if (errno != EINTR) {
                    mterror_exit(WM_CHECK_CONFIG_LOGTAG, "At main(): select(): %s", strerror(errno));
                }
                continue;

            case 0:
                continue;
        }

        if (peer = accept(sock, NULL, NULL), peer < 0) {
            if (errno != EINTR) {
                mterror(WM_CHECK_CONFIG_LOGTAG, "At main(): accept(): %s", strerror(errno));
            }
            continue;
        }

        os_calloc(OS_MAXSTR+1, sizeof(char), buffer);
        length = OS_RecvUnix(peer, OS_MAXSTR, buffer);
        switch (length) {
            case -1:
                mterror(WM_CHECK_CONFIG_LOGTAG, "At main(): OS_RecvUnix(): %s", strerror(errno));
                break;

            case 0:
                mtinfo(WM_CHECK_CONFIG_LOGTAG, "Empty message from local client.");
                // close(peer);
                break;

            case OS_MAXLEN:
                mterror(WM_CHECK_CONFIG_LOGTAG, "Received message > %i", MAX_DYN_STR);
                // close(peer);
                break;

            default:

                if(check_event_rcvd(buffer, &filetype, &filepath) < 0) {
                    break;
                }

                if(!filepath) {
                    if(strcmp(filetype, "remote")) {
                        filepath = strdup(DEFAULTDIR AGENTCONFIG);
                    } else {
                        filepath = strdup(DEFAULTCPATH);
                    }
                }

                char *output = NULL;
                test_file(filetype, filepath, &output);

                cJSON *temp_obj = cJSON_CreateObject();
                if(output) {
                    char *aux = strdup(output);

                    cJSON_AddStringToObject(temp_obj, "test_result", aux);
                    if(strstr(aux, "Test OK!") && !(strstr(aux, "ERROR"))) {
                        cJSON_AddStringToObject(temp_obj, "error_message", "none.");
                    }
                    else if(strstr(aux, "Test OK!") && strstr(aux, "ERROR")) {
                        cJSON_AddStringToObject(temp_obj, "error_message", WARN_RESULT);
                    }
                    else if(!strstr(aux, "Test OK!")) {
                        cJSON_AddStringToObject(temp_obj, "error_message", "test unsuccessful.");
                    }

                    os_free(output);
                    output = cJSON_PrintUnformatted(temp_obj);
                    os_free(aux);
                }
                else {
                    cJSON_AddStringToObject(temp_obj, "test_result", "failure testing the configuration file.");
                    output = cJSON_PrintUnformatted(temp_obj);
                }

                os_free(output);
                cJSON_Delete(temp_obj);

                break;
        }

        close(peer);
        os_free(filetype);
        os_free(filepath);
        os_free(buffer);
    }

    close(sock);
    return NULL;
}

void wm_chk_conf_destroy(){}

wmodule *wm_chk_conf_read(){
    wmodule * module;

    os_calloc(1, sizeof(wmodule), module);
    module->context = &WM_CHK_CONF_CONTEXT;
    module->tag = strdup(module->context->name);

    return module;
}

cJSON *wm_chk_conf_dump(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *wm_wd = cJSON_CreateObject();
    cJSON_AddStringToObject(wm_wd, "enabled", "yes");
    cJSON_AddItemToObject(root, "check_configuration", wm_wd);
    return root;
}

int check_event_rcvd(const char *buffer, char **filetype, char **filepath) {
    const char *jsonErrPtr;
    cJSON *event = cJSON_ParseWithOpts(buffer, &jsonErrPtr, 0);

    if(!event) {
        merror("Cannot retrieve a JSON event from buffer");
        return -1;
    }

    cJSON *operation = cJSON_GetObjectItem(event, "operation");
    if(!operation) {
        merror("'operation' field not found");
        goto fail;
    } else if(strcmp(operation->valuestring, "GET")) {
        merror("Invalid operation: '%s', at received event.", operation->valuestring);
        goto fail;
    }

    cJSON *type = cJSON_GetObjectItem(event, "type");
    if(!type) {
        merror("'type' field not found");
        goto fail;
    } else if(strcmp(type->valuestring, "request")) {
        merror("Invalid operation type: '%s', at received event.", type->valuestring);
        goto fail;
    }

    cJSON *version = cJSON_GetObjectItem(event, "version");
    if(!version) {
        merror("'version' field not found");
        goto fail;
    }
    // else if version???

    cJSON *component = cJSON_GetObjectItem(event, "component");
    if(!component) {
        merror("'component' field not found");
        goto fail;
    } else if(strcmp(component->valuestring, "check_configuration")) {
        merror("Unknown component: %s.", component->valuestring);
        goto fail;
    }

    /* Data values */
    cJSON *data = cJSON_GetObjectItem(event, "data");
    if(!data) {
        merror("'data' field not found");
        goto fail;
    }

    cJSON *data_type = cJSON_GetObjectItem(data, "type");
    if(!data_type) {
        merror("'data.type' item not found");
        goto fail;
    } else if(strcmp(data_type->valuestring, "manager") && strcmp(data_type->valuestring, "agent") && strcmp(data_type->valuestring, "remote")) {
        merror("Invalid value for data.type: %s", data_type->valuestring);
        goto fail;
    }
    *filetype = strdup(data_type->valuestring);

    cJSON *data_file = cJSON_GetObjectItem(data, "file");
    if(data_file) {
        *filepath = strdup(data_file->valuestring);
    } else {
        mwarn("'file' field not found, the default configuration file will be used.");
    }

    cJSON_Delete(event);
    return 0;

fail:
    cJSON_Delete(event);
    return -1;
}

void test_file(const char *filetype, const char *filepath, char **output) {

    int result_code;
    int timeout = 2000;
    char *output_msg = NULL;
    char cmd[OS_SIZE_6144] = {0,};
    snprintf(cmd, OS_SIZE_6144, "%s/bin/check_configuration -t %s -f %s", DEFAULTDIR, filetype, filepath);

    if (wm_exec(cmd, &output_msg, &result_code, timeout, NULL) < 0) {
        if (result_code == EXECVE_ERROR) {
            // mwarn("Path is invalid or file has insufficient permissions. %s", cmd);
            wm_strcat(output, "Path is invalid or file has insufficient permissions:", ' ');
        } else {
            // mwarn("Error executing [%s]", cmd);
            wm_strcat(output, "Error executing: ", ' ');
        }
        wm_strcat(output, cmd, ' ');
        os_free(output_msg);
        return;
    }

    if (output_msg && *output_msg) {
        // Remove last newline
        size_t lastchar = strlen(output_msg) - 1;
        output_msg[lastchar] = output_msg[lastchar] == '\n' ? '\0' : output_msg[lastchar];

        wm_strcat(output, output_msg, ' ');
    }

    os_free(output_msg);
}

#endif
#endif