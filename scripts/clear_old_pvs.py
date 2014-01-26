import sqlite3
import time

def pid_running(pid):
    #Windows specific
    import ctypes
    kernel32 = ctypes.windll.kernel32
    SYNCHRONIZE = 0x100000

    process = kernel32.OpenProcess(SYNCHRONIZE, 0, pid)
    if process != 0:
        kernel32.CloseHandle(process)
        return True
    else:
        return False

def clear_out_db(db):
    con = sqlite3.connect(db)
    con.row_factory = sqlite3.Row
    cur = con.cursor()
    cur.execute('SELECT pid , start_time FROM PVs')
    pids=[]
    rows = cur.fetchall()
    for row in rows:
        if not row[0] in pids:
            #print time.ctime(row[1])
            #Could delete based on time - e.g. don't delete anything less than a week old
            pids.append(row[0])
            
    abandoned = []
    for pid in pids:
        if pid_running(pid):
            print "Runnning"
        else:
            print "Not running"
            abandoned.append(pid)
    
    if len(abandoned) > 0:
        for pid in abandoned:
            print "deleting"
            cur.execute("DELETE FROM PVs WHERE pid = %s" % pid)
        con.commit()
        
if __name__ == '__main__':
    clear_out_db('C:/EPICS/ISIS/pvdump/PV_data.db')