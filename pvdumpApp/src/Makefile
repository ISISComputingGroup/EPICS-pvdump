TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvdump

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install pvdump.dbd into <top>/dbd
DBD += pvdump.dbd

# specify all source files to be compiled and added to the library
pvdump_SRCS += pvdump.cpp

pvdump_LIBS += $(MYSQLLIB) easySQLite sqlite utilities $(EPICS_BASE_IOC_LIBS)

ifeq ($(SHARED_LIBRARIES),YES)
USR_CPPFLAGS_WIN32 += -DSQLITE_API=__declspec(dllimport)
else
#USR_CXXFLAGS += -DCPPCONN_LIB_BUILD
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

