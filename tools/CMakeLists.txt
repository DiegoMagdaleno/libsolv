#
# CMakeLists.txt for tools
#

ADD_LIBRARY (toolstuff STATIC common_write.c)

IF (WIN32)
    INCLUDE_DIRECTORIES (${PROJECT_SOURCE_DIR}/win32/)
ENDIF ()

SET (tools_list testsolv mergesolv dumpsolv installcheck testsolv)

IF (ENABLE_RPMDB)
ADD_EXECUTABLE (rpmdb2solv rpmdb2solv.c)
TARGET_LINK_LIBRARIES (rpmdb2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (findfileconflicts findfileconflicts.c)
TARGET_LINK_LIBRARIES (findfileconflicts libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} rpmdb2solv)
ENDIF (ENABLE_RPMDB)

IF (ENABLE_RPMPKG)
ADD_EXECUTABLE (rpms2solv rpms2solv.c)
TARGET_LINK_LIBRARIES (rpms2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})
SET (tools_list ${tools_list} rpms2solv)
ENDIF (ENABLE_RPMPKG)

IF (ENABLE_RPMMD)
ADD_EXECUTABLE (repomdxml2solv repomdxml2solv.c)
TARGET_LINK_LIBRARIES (repomdxml2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (rpmmd2solv rpmmd2solv.c)
TARGET_LINK_LIBRARIES (rpmmd2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (updateinfoxml2solv updateinfoxml2solv.c)
TARGET_LINK_LIBRARIES (updateinfoxml2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (deltainfoxml2solv deltainfoxml2solv.c)
TARGET_LINK_LIBRARIES (deltainfoxml2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} repomdxml2solv rpmmd2solv updateinfoxml2solv deltainfoxml2solv)
ENDIF (ENABLE_RPMMD)

IF (ENABLE_HELIXREPO)
ADD_EXECUTABLE (helix2solv helix2solv.c)
TARGET_LINK_LIBRARIES (helix2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} helix2solv)
ENDIF (ENABLE_HELIXREPO)

IF (ENABLE_SUSEREPO)
ADD_EXECUTABLE (susetags2solv susetags2solv.c)
TARGET_LINK_LIBRARIES (susetags2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} susetags2solv)
ENDIF (ENABLE_SUSEREPO)

IF (ENABLE_COMPS)
ADD_EXECUTABLE (comps2solv comps2solv.c)
TARGET_LINK_LIBRARIES (comps2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} comps2solv)
ENDIF (ENABLE_COMPS)

IF (ENABLE_DEBIAN)
ADD_EXECUTABLE (deb2solv deb2solv.c)
TARGET_LINK_LIBRARIES (deb2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} deb2solv)
ENDIF (ENABLE_DEBIAN)

IF (ENABLE_MDKREPO)
ADD_EXECUTABLE (mdk2solv mdk2solv.c)
TARGET_LINK_LIBRARIES (mdk2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} mdk2solv)
ENDIF (ENABLE_MDKREPO)

IF (ENABLE_ARCHREPO)
ADD_EXECUTABLE (archpkgs2solv archpkgs2solv.c)
TARGET_LINK_LIBRARIES (archpkgs2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (archrepo2solv archrepo2solv.c)
TARGET_LINK_LIBRARIES (archrepo2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} archpkgs2solv archrepo2solv)
ENDIF (ENABLE_ARCHREPO)

IF (ENABLE_CUDFREPO)
ADD_EXECUTABLE (cudftest cudftest.c)
TARGET_LINK_LIBRARIES (cudftest libsolvext libsolv ${SYSTEM_LIBRARIES})
ENDIF (ENABLE_CUDFREPO)

ADD_EXECUTABLE (installcheck installcheck.c)
TARGET_LINK_LIBRARIES (installcheck libsolvext libsolv ${SYSTEM_LIBRARIES})

IF (ENABLE_APPDATA)
ADD_EXECUTABLE (appdata2solv appdata2solv.c)
TARGET_LINK_LIBRARIES (appdata2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} appdata2solv)
ENDIF (ENABLE_APPDATA)

IF (ENABLE_CONDA)
ADD_EXECUTABLE (conda2solv conda2solv.c)
TARGET_LINK_LIBRARIES (conda2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_list ${tools_list} conda2solv)
ENDIF (ENABLE_CONDA)

IF (ENABLE_WINTER)
ADD_EXECUTABLE(winter2solv winter2solv.c)
TARGET_LINK_LIBRARIES(winter2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

SET (tools_lists ${tools_list} winter2solv)
ENDIF (ENABLE_WINTER)

IF (NOT WIN32)
ADD_EXECUTABLE (repo2solv repo2solv.c )
TARGET_LINK_LIBRARIES (repo2solv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})
SET(tools_list ${tools_list} repo2solv)
ENDIF (NOT WIN32)

ADD_EXECUTABLE (dumpsolv dumpsolv.c )
TARGET_LINK_LIBRARIES (dumpsolv libsolv)

ADD_EXECUTABLE (mergesolv mergesolv.c )
TARGET_LINK_LIBRARIES (mergesolv toolstuff libsolvext libsolv ${SYSTEM_LIBRARIES})

ADD_EXECUTABLE (testsolv testsolv.c)
TARGET_LINK_LIBRARIES (testsolv libsolvext libsolv ${SYSTEM_LIBRARIES})

INSTALL (TARGETS ${tools_list} DESTINATION ${CMAKE_INSTALL_BINDIR})
