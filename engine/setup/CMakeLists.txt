IF (NOT BEOS)
    #SET_PROPERTY(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")
    SET(VEGASETTINGS_SOURCES
        src/c/setup.cpp
        src/include/central.cpp
        src/include/display_gtk.cpp
        src/include/display_dialog.cpp
        src/include/file.cpp
        src/include/general.cpp
        ../src/common/common.cpp
    )

    ADD_EXECUTABLE(vegasettings ${VEGASETTINGS_SOURCES})

#find GTK3
FIND_PACKAGE(GTK3 REQUIRED)
IF(GTK3_FOUND)
    SET(GTK_LIBS ${GTK3_LIBRARIES})
    SET(GTK_CFLAGS ${GTK3_DEFINITIONS} -DGTK)
ELSE(GTK3_FOUND)
     MESSAGE("Can't get GTK3 found")
ENDIF(GTK3_FOUND)
    ADD_DEFINITIONS(${GTK_CFLAGS})
    INCLUDE_DIRECTORIES(
        ${vsUTCS_SOURCE_DIR}/setup/src/include
        ${vsUTCS_SOURCE_DIR}/src/common
        ${vsUTCS_BINARY_DIR}
        ${GTK3_INCLUDE_DIRS}
    )
    TARGET_LINK_LIBRARIES(vegasettings ${GTK_LIBS})
ENDIF (NOT BEOS)
