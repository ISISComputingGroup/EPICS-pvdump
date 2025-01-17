TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvdump pvdump_dummy pvdump_mysql

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install pvdump.dbd into <top>/dbd
DBD += pvdump.dbd

# specify all source files to be compiled and added to the library

# we build the pvdump_mysql interface library
# on VS2022 but do not use it here, but we need
# to make pvdump_mysql.dll as we replace the one
# build on VS2010 for galil old with this version to
# work with newer mysql. 
pvdump_SRCS += pvdump.cpp pvdump_mysql.cpp
pvdump_mysql_SRCS += pvdump_mysql_int.cpp
pvdump_dummy_SRCS += pvdump_dummy.cpp

pvdump_dummy_CPPFLAGS += -DPVDUMP_DUMMY=1

pvdump_mysql_LIBS += $(MYSQLLIB)
pvdump_LIBS += easySQLite sqlite utilities pvdump_mysql $(EPICS_BASE_IOC_LIBS)
pvdump_LDFLAGS_WIN32 += /delayload:pvdump_mysql.dll
pvdump_SYS_LIBS_WIN32 += delayimp
pvdump_dummy_LIBS += easySQLite sqlite utilities $(EPICS_BASE_IOC_LIBS)

# these are used to delay load pvdump_mysql.dll when using
# the modified pvdump.cpp that calls via pvdump_mysql
#pvdump_SRCS += pvdump.cpp pvdump_mysql.cpp
#pvdump_LDFLAGS_WIN32 += /delayload:pvdump_mysql.dll
#pvdump_SYS_LIBS_WIN32 += delayimp
#pvdump_LIBS += easySQLite sqlite utilities pvdump_mysql $(EPICS_BASE_IOC_LIBS)


ifeq ($(SHARED_LIBRARIES),YES)
USR_CPPFLAGS_WIN32 += -DSQLITE_API=__declspec(dllimport)
else
USR_CXXFLAGS += -DSTATIC_CONCPP
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

pvdump_dummy.cpp : pvdump.cpp
	$(CP) $< $@
