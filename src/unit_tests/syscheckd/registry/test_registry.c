/*
 * Copyright (C) 2015-2020, Wazuh Inc.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "syscheck.h"

#define CHECK_REGISTRY_ALL                                                                             \
    CHECK_SIZE | CHECK_PERM | CHECK_OWNER | CHECK_GROUP | CHECK_MTIME | CHECK_MD5SUM | CHECK_SHA1SUM | \
    CHECK_SHA256SUM | CHECK_SEECHANGES | CHECK_TYPE

char inv_hKey[50];

static registry default_config[] = {
    { "HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", ARCH_64BIT, CHECK_REGISTRY_ALL, 320, 0, NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0", ARCH_64BIT, CHECK_REGISTRY_ALL, 0, 0, NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\Software\\Ignore", ARCH_64BIT, CHECK_REGISTRY_ALL, 320, 0, NULL, NULL },
    { inv_hKey, ARCH_64BIT, CHECK_REGISTRY_ALL, 320, 0, NULL, NULL },
    { NULL, 0, 0, 320, 0, NULL, NULL }
};

static registry one_entry_config[] = {
    { "HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", ARCH_64BIT, CHECK_REGISTRY_ALL, 320, 0, NULL, NULL }
};

static registry default_ignore[] = { { "HKEY_LOCAL_MACHINE\\Software\\Ignore", ARCH_32BIT, CHECK_REGISTRY_ALL, 0, 0, NULL, NULL },
                                     { "HKEY_LOCAL_MACHINE\\Software\\Ignore", ARCH_64BIT, CHECK_REGISTRY_ALL, 0, 0, NULL, NULL },
                                     { NULL, 0, 0, 320, 0, NULL, NULL } };

static char *default_ignore_regex_patterns[] = { "IgnoreRegex", "IgnoreRegex", NULL };

static registry_regex default_ignore_regex[] = { { NULL, ARCH_32BIT }, { NULL, ARCH_64BIT }, { NULL, 0 } };

static registry empty_config[] = { { NULL, 0, 0, 320, 0, NULL, NULL } };

extern int _base_line;

int fim_set_root_key(HKEY *root_key_handle, const char *full_key, const char **sub_key);
registry *fim_registry_configuration(const char *key, int arch);
int fim_registry_validate_path(const char *entry_path, const registry *configuration);

void expect_RegOpenKeyEx_call(HKEY hKey, LPCSTR sub_key, DWORD options, REGSAM sam, PHKEY result, LONG return_value) {
    expect_value(wrap_RegOpenKeyEx, hKey, hKey);
    expect_string(wrap_RegOpenKeyEx, lpSubKey, sub_key);
    expect_value(wrap_RegOpenKeyEx, ulOptions, options);
    expect_value(wrap_RegOpenKeyEx, samDesired, sam);
    will_return(wrap_RegOpenKeyEx, result);
    will_return(wrap_RegOpenKeyEx, return_value);
}

void expect_RegQueryInfoKey_call(DWORD sub_keys, DWORD values, PFILETIME last_write_time, LONG return_value) {
    will_return(wrap_RegQueryInfoKey, sub_keys);
    will_return(wrap_RegQueryInfoKey, values);
    will_return(wrap_RegQueryInfoKey, last_write_time);
    will_return(wrap_RegQueryInfoKey, return_value);
}

void expect_RegEnumKeyEx_call(LPSTR name, DWORD name_length, LONG return_value) {
    will_return(wrap_RegEnumKeyEx, name);
    will_return(wrap_RegEnumKeyEx, name_length);
    will_return(wrap_RegEnumKeyEx, return_value);
}

void expect_RegEnumValue_call(LPSTR value_name, DWORD type, LPBYTE data, DWORD data_length, LONG return_value) {
    will_return(wrap_RegEnumValue, value_name);
    will_return(wrap_RegEnumValue, strlen(value_name));
    will_return(wrap_RegEnumValue, type);
    will_return(wrap_RegEnumValue, data_length);
    will_return(wrap_RegEnumValue, data);
    will_return(wrap_RegEnumValue, return_value);
}

void expect_SendMSG_call(const char *message_expected, const char *locmsg_expected, char loc_expected, int ret){
    expect_string(__wrap_SendMSG, message, message_expected);
    expect_string(__wrap_SendMSG, locmsg, locmsg_expected);
    expect_value(__wrap_SendMSG, loc, loc_expected);
    will_return(__wrap_SendMSG, ret);
}

int check_fim_db_reg_key(fim_registry_key *key_to_check){
    fim_registry_key *key_saved = fim_db_get_registry_key(syscheck.database, key_to_check->path);
    if(!key_saved){
        return -1;
    }

    assert_string_equal(key_saved->perm, key_to_check->perm);
    assert_string_equal(key_saved->uid, key_to_check->uid);
    assert_string_equal(key_saved->gid, key_to_check->gid);
    assert_string_equal(key_saved->user_name, key_to_check->user_name);
    assert_string_equal(key_saved->group_name, key_to_check->group_name);

    fim_registry_free_key(key_saved);

    return 0;
}

int check_fim_db_reg_value_data(const char *name, unsigned int type, unsigned int size, const char *key_name){
    fim_registry_key *reg_key = fim_db_get_registry_key(syscheck.database, key_name);
    assert_non_null(reg_key);

    fim_registry_value_data *value_saved = fim_db_get_registry_data(syscheck.database, reg_key->id, name);
    if(!value_saved){
        fim_registry_free_key(reg_key);
        return -1;
    }

    assert_string_equal(value_saved->name, name);
    assert_int_equal(value_saved->type, type);
    assert_int_equal(value_saved->size, size);

    fim_registry_free_key(reg_key);
    fim_registry_free_value_data(value_saved);

    return 0;
}

fim_registry_key *create_reg_key(const char *path, const char *perm, const char *uid, const char *gid, const char *user_name,
                                 const char *group_name) {
    fim_registry_key *ret;

    os_calloc(1, sizeof(fim_registry_key), ret);

    ret->id = 0;
    os_strdup(path, ret->path);
    os_strdup(perm, ret->perm);
    os_strdup(uid, ret->uid);
    os_strdup(gid, ret->gid);
    os_strdup(user_name, ret->user_name);
    os_strdup(group_name, ret->group_name);

    return ret;
}

// Temporary wrapp
fim_registry_key *__wrap_fim_registry_get_key_data(HKEY key_handle, const char *path, const registry *configuration) {
    return mock_type(fim_registry_key *);
}

static int setup_group(void **state) {
    int i;
    strcpy(inv_hKey, "HKEY_LOCAL_MACHINE_Invalid_key\\Software\\Ignore");

    syscheck.registry = default_config;
    syscheck.registry_ignore = default_ignore;

    for (i = 0; default_ignore_regex_patterns[i]; i++) {
        default_ignore_regex[i].regex = calloc(1, sizeof(OSMatch));

        if (default_ignore_regex[i].regex == NULL) {
            return -1;
        }

        if (!OSMatch_Compile(default_ignore_regex_patterns[i], default_ignore_regex[i].regex, 0)) {
            return -1;
        }
    }

    syscheck.registry_ignore_regex = default_ignore_regex;

    // Init database
    syscheck.database = fim_db_init(0);

    return 0;
}

static int teardown_group(void **state) {
    int i;

    syscheck.registry = NULL;
    syscheck.registry_ignore = NULL;

    for (i = 0; syscheck.registry_ignore_regex[i].regex; i++) {
        OSMatch_FreePattern(syscheck.registry_ignore_regex[i].regex);
    }
    syscheck.registry_ignore_regex = NULL;

    // Close database
    expect_string(__wrap__mdebug1, formatted_msg, "Database transaction completed.");
    fim_db_close(syscheck.database);
    fim_db_clean();

    return 0;
}

static int setup_remove_entries(void **state) {
    syscheck.registry = empty_config;

    return 0;
}

static int teardown_restore_scan(void **state) {
    syscheck.registry = default_config;

    _base_line = 0;

    return 0;
}

static int setup_base_line(void **state) {
    syscheck.registry = one_entry_config;

    fim_registry_key **keys_array = calloc(5, sizeof(fim_registry_key*));
    if(keys_array == NULL)
        return -1;

    keys_array[0] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", "permissions", "userid", "groupid",
                                           "username", "groupname");
    keys_array[1] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\FirstSubKey", "permissions", "userid", "groupid",
                                           "username", "groupname");
    // temporary
    keys_array[2] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", "permissions", "userid", "groupid",
                                           "username", "groupname");
    keys_array[3] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\FirstSubKey", "permissions", "userid", "groupid",
                                           "username", "groupname");
    keys_array[4] = NULL;

    _base_line = 0;
    *state = keys_array;
    return 0;
}

static int teardown_clean_db_and_state(void **state) {
    char *err_msg = NULL;
    fim_registry_key **keys_array = *state;

    // DELETE TABLES
    sqlite3_exec(syscheck.database->db, "DELETE FROM registry_data;", NULL, NULL, &err_msg);
    if (err_msg) {
        fail_msg("%s", err_msg);
        sqlite3_free(err_msg);

        return -1;
    }
    sqlite3_exec(syscheck.database->db, "DELETE FROM registry_key;", NULL, NULL, &err_msg);
    if (err_msg) {
        fail_msg("%s", err_msg);
        sqlite3_free(err_msg);

        return -1;
    }

    // Free state
    if (keys_array){
        // Temporary
        for (int i = 0; keys_array[i]; ++i) {
            fim_registry_free_key(keys_array[i]);
            free(keys_array[i]);
        }
        free(keys_array);
        keys_array = NULL;
    }

    return 0;
}

static int setup_regular_scan(void **state) {
    syscheck.registry = default_config;

    fim_registry_key **keys_array = calloc(9, sizeof(fim_registry_key*));
    if(keys_array == NULL)
        return -1;

    keys_array[0] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", "permissions2", "userid2", "groupid2",
                                           "username2", "groupname2");
    keys_array[1] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\FirstSubKey", "permissions2", "userid2", "groupid2",
                                           "username2", "groupname2");
    keys_array[2] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0", "permissions3", "userid3", "groupid3",
                                           "username3", "groupname3");
    keys_array[3] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0\\depth0", "permissions3", "userid3", "groupid3",
                                           "username3", "groupname3");
    //temporary

    keys_array[4] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile", "permissions2", "userid2", "groupid2",
                                           "username2", "groupname2");
    keys_array[5] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\FirstSubKey", "permissions2", "userid2", "groupid2",
                                           "username2", "groupname2");
    keys_array[6] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0", "permissions3", "userid3", "groupid3",
                                           "username3", "groupname3");
    keys_array[7] = create_reg_key("HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0\\depth0", "permissions3", "userid3", "groupid3",
                                           "username3", "groupname3");
    keys_array[8] = NULL;

    _base_line = 1;
    *state = keys_array;
    return 0;
}

static void test_fim_set_root_key_null_root_key(void **state) {
    int ret;
    char *full_key;
    const char *sub_key;

    ret = fim_set_root_key(NULL, full_key, &sub_key);

    assert_int_equal(ret, -1);
}

static void test_fim_set_root_key_null_full_key(void **state) {
    int ret;
    HKEY root_key;
    const char *sub_key;

    ret = fim_set_root_key(&root_key, NULL, &sub_key);

    assert_int_equal(ret, -1);
}

static void test_fim_set_root_key_null_sub_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key;

    ret = fim_set_root_key(&root_key, full_key, NULL);

    assert_int_equal(ret, -1);
}

static void test_fim_set_root_key_invalid_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "This wont match to any root key";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, -1);
    assert_null(root_key);
}

static void test_fim_set_root_key_invalid_root_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "HKEY_LOCAL_MACHINE_This_is_almost_valid\\but\\not\\quite\\valid";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, -1);
    assert_null(root_key);
}

static void test_fim_set_root_key_valid_HKEY_LOCAL_MACHINE_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "HKEY_LOCAL_MACHINE\\This\\is_a_valid\\key";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, 0);
    assert_ptr_equal(root_key, HKEY_LOCAL_MACHINE);
    assert_ptr_equal(sub_key, full_key + 19);
}

static void test_fim_set_root_key_valid_HKEY_CLASSES_ROOT_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "HKEY_CLASSES_ROOT\\This\\is_a_valid\\key";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, 0);
    assert_ptr_equal(root_key, HKEY_CLASSES_ROOT);
    assert_ptr_equal(sub_key, full_key + 18);
}

static void test_fim_set_root_key_valid_HKEY_CURRENT_CONFIG_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "HKEY_CURRENT_CONFIG\\This\\is_a_valid\\key";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, 0);
    assert_ptr_equal(root_key, HKEY_CURRENT_CONFIG);
    assert_ptr_equal(sub_key, full_key + 20);
}

static void test_fim_set_root_key_valid_HKEY_USERS_key(void **state) {
    int ret;
    HKEY root_key;
    char *full_key = "HKEY_USERS\\This\\is_a_valid\\key";
    const char *sub_key;

    ret = fim_set_root_key(&root_key, full_key, &sub_key);

    assert_int_equal(ret, 0);
    assert_ptr_equal(root_key, HKEY_USERS);
    assert_ptr_equal(sub_key, full_key + 11);
}

static void test_fim_registry_configuration_registry_found(void **state) {
    registry *configuration;

    configuration = fim_registry_configuration("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\something", ARCH_64BIT);
    assert_non_null(configuration);
    assert_string_equal(configuration->entry, "HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile");
    assert_int_equal(configuration->arch, ARCH_64BIT);
}

static void test_fim_registry_configuration_registry_not_found_arch_does_not_match(void **state) {
    registry *configuration;

    expect_any_always(__wrap__mdebug2, formatted_msg);

    configuration = fim_registry_configuration("HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\something", ARCH_32BIT);
    assert_null(configuration);
}

static void test_fim_registry_configuration_registry_not_found_path_does_not_match(void **state) {
    registry *configuration;

    expect_any_always(__wrap__mdebug2, formatted_msg);

    configuration = fim_registry_configuration("HKEY_LOCAL_MACHINE\\Software\\Classes\\something", ARCH_64BIT);
    assert_null(configuration);
}

static void test_fim_registry_configuration_null_key(void **state) {
    registry *configuration;

    expect_any_always(__wrap__mdebug2, formatted_msg);

    configuration = fim_registry_configuration(NULL, ARCH_64BIT);
    assert_null(configuration);
}

static void test_fim_registry_validate_path_null_configuration(void **state) {
    char *path = "HKEY_LOCAL_MACHINE\\Software\\Classes\\something";
    int ret;

    ret = fim_registry_validate_path(path, NULL);

    assert_int_equal(ret, -1);
}

static void test_fim_registry_validate_path_null_entry_path(void **state) {
    registry *configuration = &syscheck.registry[0];
    int ret;

    ret = fim_registry_validate_path(NULL, configuration);

    assert_int_equal(ret, -1);
}

static void test_fim_registry_validate_path_valid_entry_path(void **state) {
    char *path = "HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\Some\\valid\\path";
    registry *configuration = &syscheck.registry[0];
    int ret;

    ret = fim_registry_validate_path(path, configuration);

    assert_int_equal(ret, 0);
}

static void test_fim_registry_validate_path_invalid_recursion_level(void **state) {
    char *path = "HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0\\This\\must\\fail";
    registry *configuration = &syscheck.registry[1];
    int ret;
    expect_string(__wrap__mdebug2, formatted_msg, "(6217): Maximum level of recursion reached. Depth:2 recursion_level:0 'HKEY_LOCAL_MACHINE\\Software\\RecursionLevel0\\This\\must\\fail'");

    ret = fim_registry_validate_path(path, configuration);

    assert_int_equal(ret, -1);
}

static void test_fim_registry_validate_path_ignore_entry(void **state) {
    char *path = "HKEY_LOCAL_MACHINE\\Software\\Ignore";
    registry *configuration = &syscheck.registry[2];
    int ret;

    expect_string(__wrap__mdebug2, formatted_msg, "(6204): Ignoring 'registry' 'HKEY_LOCAL_MACHINE\\Software\\Ignore' due to 'HKEY_LOCAL_MACHINE\\Software\\Ignore'");

    ret = fim_registry_validate_path(path, configuration);

    assert_int_equal(ret, -1);
}

static void test_fim_registry_validate_path_regex_ignore_entry(void **state) {
    char *path = "HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\IgnoreRegex\\This\\must\\fail";
    registry *configuration = &syscheck.registry[0];
    int ret;

    expect_string(__wrap__mdebug2, formatted_msg, "(6205): Ignoring 'registry' 'HKEY_LOCAL_MACHINE\\Software\\Classes\\batfile\\IgnoreRegex\\This\\must\\fail' due to sregex 'IgnoreRegex'");

    ret = fim_registry_validate_path(path, configuration);

    assert_int_equal(ret, -1);
}

static void test_fim_registry_scan_no_entries_configured(void **state) {
    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_START);
    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_ENDED);

    fim_registry_scan();

    assert_int_equal(_base_line, 1);
}

static void test_fim_registry_scan_base_line_generation(void **state) {
    fim_registry_key **keys_array = *state;
    int ret = 0;

    char value_name[10] = "valuename";
    unsigned int value_type = REG_DWORD;
    unsigned int value_size = 4;
    DWORD value_data = 123456;

    FILETIME last_write_time = { 0, 1000 };

    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_START);
    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_ENDED);
    expect_any_always(__wrap__mdebug2, formatted_msg);

    // Temporary
    will_return(__wrap_fim_registry_get_key_data, keys_array[1]);
    will_return(__wrap_fim_registry_get_key_data, keys_array[0]);

    // Scan a subkey of batfile
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\Classes\\batfile", 0, KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(1, 0, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumKeyEx_call("FirstSubKey", 12, ERROR_SUCCESS);

    // Scan a value of FirstSubKey
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\Classes\\batfile\\FirstSubKey", 0,
                             KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(0, 1, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumValue_call(value_name, value_type, (LPBYTE)&value_data, value_size, ERROR_SUCCESS);

    fim_registry_scan();
    assert_int_equal(_base_line, 1);

    // Database check
    ret = check_fim_db_reg_key(keys_array[2]);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_key(keys_array[3]);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_value_data("valuename", REG_DWORD, 4, keys_array[3]->path);
    assert_int_equal(ret, 0);
}

static void test_fim_registry_scan_regular_scan(void **state) {
    fim_registry_key **keys_array = *state;
    int ret = 0;

    char value_name[10] = "valuename";
    unsigned int value_type = REG_DWORD;
    unsigned int value_size = 4;
    DWORD value_data = 123456;

    FILETIME last_write_time = { 0, 1000 };

    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_START);
    expect_string(__wrap__mdebug1, formatted_msg, "(6919): Invalid syscheck registry entry: 'HKEY_LOCAL_MACHINE_Invalid_key\\Software\\Ignore' arch: '[x64] '.");
    expect_string(__wrap__mdebug1, formatted_msg, FIM_WINREGISTRY_ENDED);
    expect_any_always(__wrap__mdebug2, formatted_msg);

    // Temporary
    will_return(__wrap_fim_registry_get_key_data, keys_array[1]);
    will_return(__wrap_fim_registry_get_key_data, keys_array[0]);
    will_return(__wrap_fim_registry_get_key_data, keys_array[3]);
    will_return(__wrap_fim_registry_get_key_data, keys_array[2]);

    // Scan a subkey of batfile
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\Classes\\batfile", 0, KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(1, 0, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumKeyEx_call("FirstSubKey", 12, ERROR_SUCCESS);

    // Scan a value of FirstSubKey
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\Classes\\batfile\\FirstSubKey", 0,
                             KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(0, 1, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumValue_call(value_name, value_type, (LPBYTE)&value_data, value_size, ERROR_SUCCESS);

    // Scan a subkey of RecursionLevel0
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\RecursionLevel0", 0, KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(1, 0, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumKeyEx_call("depth0", 7, ERROR_SUCCESS);

    // Scan a subkey of depth0
    expect_RegOpenKeyEx_call(HKEY_LOCAL_MACHINE, "Software\\RecursionLevel0\\depth0", 0,
                             KEY_READ | KEY_WOW64_64KEY, NULL, ERROR_SUCCESS);
    expect_RegQueryInfoKey_call(1, 0, &last_write_time, ERROR_SUCCESS);

    expect_RegEnumKeyEx_call("depth1", 7, ERROR_SUCCESS);

    fim_registry_scan();

    // Database check
    ret = check_fim_db_reg_key(keys_array[4]);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_key(keys_array[5]);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_value_data("valuename", REG_DWORD, 4, keys_array[5]->path);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_key(keys_array[6]);
    assert_int_equal(ret, 0);
    ret = check_fim_db_reg_key(keys_array[7]);
    assert_int_equal(ret, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        /* fim_set_root_key test */
        cmocka_unit_test(test_fim_set_root_key_null_root_key),
        cmocka_unit_test(test_fim_set_root_key_null_full_key),
        cmocka_unit_test(test_fim_set_root_key_null_sub_key),
        cmocka_unit_test(test_fim_set_root_key_invalid_key),
        cmocka_unit_test(test_fim_set_root_key_invalid_root_key),
        cmocka_unit_test(test_fim_set_root_key_valid_HKEY_LOCAL_MACHINE_key),
        cmocka_unit_test(test_fim_set_root_key_valid_HKEY_CLASSES_ROOT_key),
        cmocka_unit_test(test_fim_set_root_key_valid_HKEY_CURRENT_CONFIG_key),
        cmocka_unit_test(test_fim_set_root_key_valid_HKEY_USERS_key),

        /* fim_registry_configuration tests */
        cmocka_unit_test(test_fim_registry_configuration_registry_found),
        cmocka_unit_test(test_fim_registry_configuration_registry_not_found_arch_does_not_match),
        cmocka_unit_test(test_fim_registry_configuration_registry_not_found_path_does_not_match),
        cmocka_unit_test(test_fim_registry_configuration_null_key),

        /* fim_registry_validate_path tests */
        cmocka_unit_test(test_fim_registry_validate_path_null_configuration),
        cmocka_unit_test(test_fim_registry_validate_path_null_entry_path),
        cmocka_unit_test(test_fim_registry_validate_path_valid_entry_path),
        cmocka_unit_test(test_fim_registry_validate_path_invalid_recursion_level),
        cmocka_unit_test(test_fim_registry_validate_path_ignore_entry),
        cmocka_unit_test(test_fim_registry_validate_path_regex_ignore_entry),

        /* fim_registry_scan tests */
        cmocka_unit_test_setup_teardown(test_fim_registry_scan_no_entries_configured, setup_remove_entries, teardown_restore_scan),
        cmocka_unit_test_setup_teardown(test_fim_registry_scan_base_line_generation, setup_base_line, teardown_clean_db_and_state),
        cmocka_unit_test_setup_teardown(test_fim_registry_scan_regular_scan, setup_regular_scan, teardown_clean_db_and_state),
    };

    return cmocka_run_group_tests(tests, setup_group, teardown_group);
}