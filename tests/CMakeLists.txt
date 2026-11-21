ecm_add_tests(
    tst_sshhost.cpp
    tst_sshconfigparser.cpp
    tst_sshconfigwriter.cpp
    tst_sshconfigloader.cpp
    tst_sshsession.cpp
    tst_sshagent.cpp
    tst_sshagentclient.cpp
    LINK_LIBRARIES konsolesshmanager_core Qt${QT_MAJOR_VERSION}::Test
)

ecm_add_test(tst_apptheme.cpp LINK_LIBRARIES konsolesshmanager_ui Qt${QT_MAJOR_VERSION}::Test)
set_tests_properties(tst_apptheme PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    add_executable(tst_mainwindow tst_mainwindow.cpp)
    target_link_libraries(tst_mainwindow PRIVATE konsolesshmanager_ui Qt${QT_MAJOR_VERSION}::DBus Qt${QT_MAJOR_VERSION}::Test)
    add_test(NAME tst_mainwindow
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} --config-file=${CMAKE_CURRENT_SOURCE_DIR}/data/dbus-test-session.conf -- $<TARGET_FILE:tst_mainwindow>)
    set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=;KSSHM_TEST_PRIVATE_BUS=1")
endif()

target_compile_definitions(tst_sshconfigparser PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# End-to-end tests of the helper binary, run against a fake ssh placed first in PATH.
target_compile_definitions(tst_sshsession PRIVATE SSH_SESSION_HELPER="$<TARGET_FILE:konsole-ssh-session>")
add_dependencies(tst_sshsession konsole-ssh-session)
# The askpass relay, run by ssh-add in place of the app.
add_executable(askpassrelay askpassrelay.cpp)
target_link_libraries(askpassrelay PRIVATE konsolesshmanager_core)
target_compile_definitions(tst_sshagentclient PRIVATE ASKPASS_RELAY="$<TARGET_FILE:askpassrelay>")
add_dependencies(tst_sshagentclient askpassrelay)
