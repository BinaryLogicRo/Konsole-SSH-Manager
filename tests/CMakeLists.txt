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

target_compile_definitions(tst_sshconfigparser PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# End-to-end tests of the helper binary, run against a fake ssh placed first in PATH.
target_compile_definitions(tst_sshsession PRIVATE SSH_SESSION_HELPER="$<TARGET_FILE:konsole-ssh-session>")
add_dependencies(tst_sshsession konsole-ssh-session)
# The askpass relay, run by ssh-add in place of the app.
add_executable(askpassrelay askpassrelay.cpp)
target_link_libraries(askpassrelay PRIVATE konsolesshmanager_core)
target_compile_definitions(tst_sshagentclient PRIVATE ASKPASS_RELAY="$<TARGET_FILE:askpassrelay>")
add_dependencies(tst_sshagentclient askpassrelay)
