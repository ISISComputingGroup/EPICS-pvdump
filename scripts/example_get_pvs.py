import socket
import getpass
from epics import PV

prefix = socket.gethostname().upper() + ":"
user = getpass.getuser()

#Get all the PVs (limited by waveform size!)
pv = PV(prefix + 'PVS')
pv.put('junk', wait=True)
pvs = pv.char_value.split(';')
print pvs

#Get PVs for one IOC
pv = PV(prefix + 'IOC')
pv.put(prefix + user + ':SIMPLE', wait=True)
pvs = pv.char_value.split(';')
print pvs
