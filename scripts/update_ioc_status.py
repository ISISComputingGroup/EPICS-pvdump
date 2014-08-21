import sqlite3
import time
import psutil
        
def pid_matched(pid, path):
    #Check the PID exists and the PATH match
    exe_path = ""
    procs = psutil.get_process_list()
    for proc in procs:
        if proc.pid == pid:
            exe_path = str(proc.exe)
            break
    return exe_path.lower() == path.lower()    

def update_running(db):
    con = sqlite3.connect(db)
    con.row_factory = sqlite3.Row
    cur = con.cursor()
    cur.execute('SELECT * FROM IOCs')
    while True:
        row = cur.fetchone()
        if row == None:
            break

        #Check the PID exists and the PATH match   
        if pid_matched(row['pid'], row['exe_path']):
            #Has it changed to running?
            if row['running'] != 1:
                #Yes - the IOC should automatically set its running value to 1 (i.e. running)
                #so this code probably will not be called
                print row['ioc'], "changed to running"
                cur.execute("UPDATE IOCs SET Running=? WHERE pid=?", (1, row['pid']))
                con.commit()                
        else:
            #Either the PID does not exist or the path does not match
            #Has it stopped since last check?
            if row['running'] == 1:
                #Yes
                print row['ioc'], "changed to not running"
                cur.execute("UPDATE IOCs SET Running=? WHERE pid=?", (0, row['pid']))
                con.commit()    

        
if __name__ == '__main__':
    update_running('C:/EPICS/ISIS/pvdump/PV_data.db')