TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvdump pvdump_dummy

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install pvdump.dbd into <top>/dbd
DBD += pvdump.dbd

# specify all source files to be compiled and added to the library
pvdump_SRCS += pvdump.cpp
pvdump_dummy_SRCS += pvdump_dummy.cpp

pvdump_dummy_CPPFLAGS += -DPVDUMP_DUMMY=1

pvdump_LIBS += $(MYSQLLIB) easySQLite sqlite utilities $(EPICS_BASE_IOC_LIBS)
pvdump_dummy_LIBS += easySQLite sqlite utilities $(EPICS_BASE_IOC_LIBS)

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
