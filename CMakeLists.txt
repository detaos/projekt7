project(projekt7)
 
find_package(KDE4 REQUIRED)
include_directories(${KDE4_INCLUDES})
 
set(projekt7_SRCS 
  main.cpp
  player.cpp
)

kde4_add_executable(projekt7 ${projekt7_SRCS})
kde4_install_icons(${ICON_INSTALL_DIR})

target_link_libraries(projekt7 tag sqlite3 QtSql
                               ${KDE4_KDEUI_LIBS}
                               ${KDE4_KIO_LIBS}
							   ${KDE4_PHONON_LIBS})
 
install(TARGETS projekt7 DESTINATION ${BIN_INSTALL_DIR})
install(FILES projekt7ui.rc
		DESTINATION ${DATA_INSTALL_DIR}/projekt7)
