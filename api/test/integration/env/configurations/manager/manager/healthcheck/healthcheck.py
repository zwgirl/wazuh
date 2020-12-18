import os
import socket


def check(result):
    if "differ" in result:
        return 1
    else:
        return 0


def get_master_health():
    os.system("/var/ossec/bin/ossec-control status > /tmp/daemons.txt")
    check0 = check(os.popen("diff -q /tmp/daemons.txt /configuration_files/healthcheck/master_daemons_check.txt").read())
    check1 = 0 if os.system("grep -qs 'Listening on ' /var/ossec/logs/api.log") == 0 else 1
    return check0 or check1


def get_worker_health():
    os.system("/var/ossec/bin/ossec-control status > /tmp/daemons.txt")
    check0 = check(os.popen("diff -q /tmp/daemons.txt /configuration_files/healthcheck/worker_daemons_check.txt").read())
    check1 = 0 if os.system("grep -qs 'Listening on ' /var/ossec/logs/api.log") == 0 else 1
    return check0 or check1


if __name__ == "__main__":
    exit(get_master_health()) if socket.gethostname() == 'wazuh-master' else exit(get_worker_health())
