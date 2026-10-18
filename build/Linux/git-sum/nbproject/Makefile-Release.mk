#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Release
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/7ea65360/GitSumApp.o \
	${OBJECTDIR}/_ext/7ea65360/GitSumConfig.o \
	${OBJECTDIR}/_ext/7ea65360/GitSumFrame.o \
	${OBJECTDIR}/_ext/7ea65360/GitStatus.o \
	${OBJECTDIR}/_ext/7ea65360/GitLog.o \
	${OBJECTDIR}/_ext/7ea65360/GitFetch.o \
	${OBJECTDIR}/_ext/7ea65360/GitDiff.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=`wx-config --cxxflags` `pkg-config --cflags libgit2`
CXXFLAGS=`wx-config --cxxflags` `pkg-config --cflags libgit2`

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=`wx-config --libs` `pkg-config --libs libgit2`

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/git-sum

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/git-sum: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/git-sum ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/_ext/7ea65360/GitSumApp.o: ../../../GitSumApp.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitSumApp.o ../../../GitSumApp.cpp

${OBJECTDIR}/_ext/7ea65360/GitSumConfig.o: ../../../GitSumConfig.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitSumConfig.o ../../../GitSumConfig.cpp

${OBJECTDIR}/_ext/7ea65360/GitSumFrame.o: ../../../GitSumFrame.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitSumFrame.o ../../../GitSumFrame.cpp

${OBJECTDIR}/_ext/7ea65360/GitStatus.o: ../../../GitStatus.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitStatus.o ../../../GitStatus.cpp

${OBJECTDIR}/_ext/7ea65360/GitLog.o: ../../../GitLog.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitLog.o ../../../GitLog.cpp

${OBJECTDIR}/_ext/7ea65360/GitFetch.o: ../../../GitFetch.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitFetch.o ../../../GitFetch.cpp

${OBJECTDIR}/_ext/7ea65360/GitDiff.o: ../../../GitDiff.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/7ea65360
	${RM} "$@.d"
	$(COMPILE.cc) -O2 -I../../.. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/7ea65360/GitDiff.o ../../../GitDiff.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
