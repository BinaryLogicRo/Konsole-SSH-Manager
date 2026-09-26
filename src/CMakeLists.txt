# Display-free core: SSH config parsing, writing and loading, and the session
# helper logic. Tests link against this.
add_library(konsolesshmanager_core STATIC
    logging.cpp
    sshconfig/sshhost.cpp
    sshconfig/sshconfigdocument.cpp
    sshconfig/sshconfigparser.cpp
    sshconfig/sshconfigwriter.cpp
    sshconfig/sshconfigloader.cpp
    sshconfig/sshkeywords.cpp
    sshconfig/sshresolver.cpp
    sshconfig/hoststore.cpp
    sshsession.cpp
)
target_include_directories(konsolesshmanager_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(konsolesshmanager_core PUBLIC Qt${QT_MAJOR_VERSION}::Core KF${KF_MAJOR_VERSION}::I18n)

# Run inside each terminal tab: starts `ssh <alias>` and keeps the tab open on failure.
add_executable(konsole-ssh-session sshsessionmain.cpp)
target_link_libraries(konsole-ssh-session PRIVATE konsolesshmanager_core)

# The app's UI, shared by the app and the optional README screenshot tool (tools/screenshot).
add_library(konsolesshmanager_ui STATIC
    mainwindow.cpp
    hostsidebar.cpp
    hostoperations.cpp
    terminaltab.cpp
    models/hosttreemodel.cpp
    dialogs/hosteditdialog.cpp
    dialogs/effectivesettingsdialog.cpp
)
target_link_libraries(konsolesshmanager_ui PUBLIC
    konsolesshmanager_core
    Qt${QT_MAJOR_VERSION}::Widgets
    KF${KF_MAJOR_VERSION}::Parts
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::I18n
)

add_executable(konsole-ssh-manager
    main.cpp
    resources.qrc
)
target_compile_definitions(konsole-ssh-manager PRIVATE KSSHM_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(konsole-ssh-manager PRIVATE konsolesshmanager_ui)
# The app starts the helper from its own directory, so build it along with the app.
add_dependencies(konsole-ssh-manager konsole-ssh-session)

# The app looks for the helper in its own directory, so both go to the same place.
install(TARGETS konsole-ssh-manager konsole-ssh-session RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
