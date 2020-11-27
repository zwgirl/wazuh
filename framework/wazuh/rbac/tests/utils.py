import os
from importlib import reload
from unittest.mock import patch

from sqlalchemy import create_engine
from sqlalchemy.exc import OperationalError
from sqlalchemy.orm import sessionmaker


def create_memory_db(sql_file, session, test_data_path):
    with open(os.path.join(test_data_path, sql_file)) as f:
        for line in f.readlines():
            line = line.strip()
            if '* ' not in line and '/*' not in line and '*/' not in line and line != '':
                session.execute(line)
                #session.commit()


def init_db(schema, test_data_path):
    with patch('wazuh.core.common.ossec_uid'), patch('wazuh.core.common.ossec_gid'):
        with patch('sqlalchemy.create_engine', return_value=create_engine("sqlite://")):
            with patch('shutil.chown'), patch('os.chmod'):
                import wazuh.rbac.orm as orm
                # Reload the ORM so database will be purged
                reload(orm)
                try:
                    orm.db_manager.connect("test_database")
                    orm.db_manager.create_database("test_database")
                    orm.db_manager.insert_data_from_yaml("test_database")
                    create_memory_db(schema, orm.db_manager.sessions["test_database"], test_data_path)
                    return orm.db_manager.sessions["test_database"]
                except OperationalError:
                    print("operational error")
                    pass
