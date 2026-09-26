ecm_add_tests(
    tst_sshhost.cpp
    tst_sshconfigparser.cpp
    tst_sshconfigwriter.cpp
    tst_sshconfigloader.cpp
    LINK_LIBRARIES konsolesshmanager_core Qt${QT_MAJOR_VERSION}::Test
)

target_compile_definitions(tst_sshconfigparser PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
