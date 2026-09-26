# Display-free core: SSH config parsing, writing and loading. Tests link against this.
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
)
target_include_directories(konsolesshmanager_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(konsolesshmanager_core PUBLIC Qt${QT_MAJOR_VERSION}::Core KF${KF_MAJOR_VERSION}::I18n)

add_executable(konsole-ssh-manager
    main.cpp
    mainwindow.cpp
    hostsidebar.cpp
    hostoperations.cpp
    terminaltab.cpp
    models/hosttreemodel.cpp
    dialogs/hosteditdialog.cpp
    dialogs/effectivesettingsdialog.cpp
)
target_compile_definitions(konsole-ssh-manager PRIVATE KSSHM_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(konsole-ssh-manager PRIVATE
    konsolesshmanager_core
    Qt${QT_MAJOR_VERSION}::Widgets
    KF${KF_MAJOR_VERSION}::Parts
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::I18n
)
