#!/usr/bin/env python
# Copyright (C) 2015-2019, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is a free software; you can redistribute it and/or modify it under the terms of GPLv2
import json
import os
import pytest
from unittest.mock import patch, mock_open

from wazuh.exception import WazuhException
from wazuh.manager import upload_file, get_file, restart, validation, status, delete_file


test_data_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data')


class InitManager:
    def __init__(self):
        """
        Sets up necessary environment to test manager functions
        """
        # path for temporary API files
        self.api_tmp_path = os.path.join(test_data_path, 'tmp')
        # rules
        self.input_rules_file = 'test_rules.xml'
        self.output_rules_file = 'uploaded_test_rules.xml'
        # decoders
        self.input_decoders_file = 'test_decoders.xml'
        self.output_decoders_file = 'uploaded_test_decoders.xml'
        # CDB lists
        self.input_lists_file = 'test_lists'
        self.output_lists_file = 'uploaded_test_lists'


@pytest.fixture(scope='module')
def test_manager():
    # Set up
    test_manager = InitManager()
    return test_manager


@pytest.mark.parametrize('process_status', [
    'running',
    'stopped',
    'failed',
    'restarting',
    'starting'
])
@patch('wazuh.manager.exists')
@patch('wazuh.manager.glob')
def test_status(manager_glob, manager_exists, test_manager, process_status):
    """
    Tests manager.status() function in two cases:
        * PID files are created and processed are running,
        * No process is running and therefore no PID files have been created
    :param manager_glob: mock of glob.glob function
    :param manager_exists: mock of os.path.exists function
    :param test_manager: pytest fixture
    :param process_status: status to test (valid values: running/stopped/failed/restarting).
    :return:
    """
    def mock_glob(path_to_check):
        return [path_to_check.replace('*', '0234')] if process_status == 'running' else []

    def mock_exists(path_to_check):
        if path_to_check == '/proc/0234':
            return process_status == 'running'
        else:
            return path_to_check.endswith(f'.{process_status.replace("ing","").replace("re", "")}') or \
                   path_to_check.endswith(f'.{process_status.replace("ing","")}')

    manager_glob.side_effect = mock_glob
    manager_exists.side_effect = mock_exists
    manager_status = status()
    assert isinstance(manager_status, dict)
    assert all(process_status == x for x in manager_status.values())
    if process_status == 'running':
        manager_exists.assert_any_call("/proc/0234")


@patch('socket.socket')
def test_restart_ok(test_manager):
    """
    Tests restarting a manager
    """
    assert restart() == 'Restarting manager'


@pytest.mark.parametrize('input_file, output_file', [
    ('input_rules_file', 'output_rules_file'),
    ('input_decoders_file', 'output_decoders_file'),
    ('input_lists_file', 'output_lists_file')
])
@patch('wazuh.common.ossec_path', new=test_data_path)
@patch('time.time', return_value=0)
@patch('random.randint', return_value=0)
@patch('wazuh.manager.chmod')
@patch('wazuh.manager.move')
@patch('wazuh.manager.remove')
def test_upload_file(remove_mock, move_mock, chmod_mock, mock_rand, mock_time, test_manager, input_file, output_file):
    """
    Tests uploading a file to the manager
    """
    input_file, output_file = getattr(test_manager, input_file), getattr(test_manager, output_file)

    with open(os.path.join(test_data_path, input_file)) as f:
        xml_file = f.read()
    m = mock_open(read_data=xml_file)
    with patch('builtins.open', m):
        upload_file(input_file, output_file, 'application/xml')

    m.assert_any_call(os.path.join(test_manager.api_tmp_path, 'api_tmp_file_0_0.xml'))
    m.assert_any_call(os.path.join(test_manager.api_tmp_path, 'api_tmp_file_0_0.xml'), 'w')
    m.assert_any_call(os.path.join(test_data_path, input_file))
    move_mock.assert_called_once_with(os.path.join(test_manager.api_tmp_path, 'api_tmp_file_0_0.xml'),
                                      os.path.join(test_data_path, output_file))
    remove_mock.assert_called_once_with(os.path.join(test_data_path, input_file))


@patch('wazuh.manager.exists', return_value=False)
def test_restart_ko_socket(test_manager):
    """
    Tests restarting a manager when the socket is not created
    """
    with pytest.raises(WazuhException, match='.* 1901 .*'):
        restart()


@pytest.mark.parametrize('input_file', [
    'input_rules_file',
    'input_decoders_file',
    'input_lists_file'
])
@patch('wazuh.common.ossec_path', test_data_path)
def test_get_file(test_manager, input_file):
    input_file = getattr(test_manager, input_file)
    with open(os.path.join(test_data_path, input_file)) as f:
        xml_file = f.read()

    with patch('builtins.open', mock_open(read_data=xml_file)):
        result = get_file(input_file)
    assert result == xml_file


@pytest.mark.parametrize('error_flag, error_msg', [
    (0, ""),
    (1, "2019/02/27 11:30:07 wazuh-clusterd: ERROR: [Cluster] [Main] Error 3004 - Error in cluster configuration: "
        "Unspecified key"),
    (1, "2019/02/27 11:30:24 ossec-authd: ERROR: (1230): Invalid element in the configuration: "
        "'use_source_i'.\n2019/02/27 11:30:24 ossec-authd: ERROR: (1202): Configuration error at "
        "'/var/ossec/etc/ossec.conf'.")
])
def test_validation(test_manager, error_flag, error_msg):
    """
    Tests configuration validation function with multiple scenarios:
        * No errors found in configuration
        * Error found in cluster configuration
        * Error found in any other configuration
    """
    json_response = json.dumps({'error': error_flag, 'message': error_msg}).encode()
    with patch('socket.socket') as sock:
        sock.return_value.recv.return_value = json_response
        result = validation()
    assert result['status'] == ('KO' if error_flag > 0 else 'OK')
    if error_flag:
        assert all(map(lambda x: x[0] in x[1], zip(result['details'], error_msg.split('\n'))))


def test_delete_file(test_manager):
    """
    Tests delete_file function and all possible scenarios
    """
    with patch('wazuh.manager.exists', return_value=True):
        with patch('wazuh.manager.remove'):
            assert(isinstance(delete_file('/test/file'), str))
        with patch('wazuh.manager.remove', side_effect=IOError()):
            with pytest.raises(WazuhException, match='.* 1907 .*'):
                delete_file('/test/file')
    with patch('wazuh.manager.exists', return_value=False):
        with pytest.raises(WazuhException, match='.* 1906 .*'):
            delete_file('/test/file')
