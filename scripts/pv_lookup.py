from pcaspy import Driver, SimpleServer
import time
import random
import threading
import sqlite3
import socket

import update_ioc_status

pvdb = {
    'PVS' : {
        'type' : 'char',
        'count' : 10000,
    },
    'IOC' : {
        'type' : 'char',
        'count' : 5000,
    },
    'IOCS' : {
        'type' : 'char',
        'count' : 5000,
    }
}

class GetPVDriver(Driver):
    def __init__(self, dbfile):
        super(GetPVDriver, self).__init__()
        self._dbfile = dbfile

    def read(self, reason):
        if reason == 'PVS':
            value = self.__get_pvs_from_sqlite(self._dbfile)   
        elif reason == 'IOCS':
            value = self.__get_iocs_from_sqlite(self._dbfile)
        else:
            value = self.getParam(reason)
        return value
        
    def write(self, reason, value):
        status = True
        if reason == 'PVS':
            value = self.__get_pvs_from_sqlite(self._dbfile)
        elif reason == 'IOC':
            value = self.__get_pvs_from_sqlite(self._dbfile, value)
        elif reason == 'IOCS':
            value = self.__get_iocs_from_sqlite(self._dbfile)
        else:
            status = False
        # store the values
        if status:
            self.setParam(reason, value)
            self.updatePVs()

        return status
        
    def __get_pvs_from_sqlite(self, db, ioc=""):
        #TODO: Check file exists and add exception handling
        con = sqlite3.connect(db)
        cur = con.cursor()
        if ioc == "":
            cur.execute('SELECT pvname FROM PVs')
        else:
            cur.execute("SELECT pvname FROM PVs WHERE ioc = '%s'" % ioc)
        pvs = []
        while True:
            row = cur.fetchone()
            if row == None:
                break
            pvs.append(row[0])
        ans = ';'.join(pvs)
        #Must be ascii?
        return ans.encode('ascii', 'ignore')
        
    def __get_iocs_from_sqlite(self, db):
        #TODO: Check file exists and add exception handling
        
        #This should not really be here as it will slow down ca if it has to wait for
        #the db to allow it to write - it probably should run periodically on a separate thread
        update_ioc_status.update_running(db)
        
        con = sqlite3.connect(db)
        con.row_factory = sqlite3.Row
        cur = con.cursor()
        cur.execute('SELECT * FROM IOCs')
        iocs = []
        while True:
            row = cur.fetchone()
            if row == None:
                break
            iocs.append(row['ioc'] + "|" + str(row['running']) )
        ans = ';'.join(iocs)
        #Must be ascii?
        return ans.encode('ascii', 'ignore')


if __name__ == '__main__':
    prefix = socket.gethostname().upper() + ":"
    server = SimpleServer()
    server.createPV(prefix, pvdb)
    driver = GetPVDriver('C:/EPICS/ISIS/pvdump/PV_data.db')

    # process CA transactions
    while True:
        server.process(0.1)
