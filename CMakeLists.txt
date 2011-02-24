project(projekt7)
 
find_package(KDE4 REQUIRED)
include_directories(${KDE4_INCLUDES})
 
set(projekt7_SRCS 
  main.cpp
  player.cpp
)

kde4_add_executable(projekt7 ${projekt7_SRCS})
kde4_add_app_icon(projekt7_SRCS
				  "${KDE4_INSTALL_DIR}/share/icons/hicolor/*/apps/projekt7.png")
add_subdirectory(icons)

target_link_libraries(projekt7 tag sqlite3
                      ${KDE4_KDEUI_LIBS}
                      ${KDE4_KIO_LIBS}
					  ${KDE4_PHONON_LIBS})

install(TARGETS projekt7 DESTINATION ${BIN_INSTALL_DIR})
install(FILES projekt7ui.rc DESTINATION ${DATA_INSTALL_DIR}/projekt7)
install(PROGRAMS projekt7.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
