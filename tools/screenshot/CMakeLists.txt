add_executable(konsole-ssh-manager-screenshot screenshot.cpp)
target_link_libraries(konsole-ssh-manager-screenshot PRIVATE konsolesshmanager_ui)
target_compile_definitions(konsole-ssh-manager-screenshot PRIVATE
    KSSHM_ICON_FILE="${PROJECT_SOURCE_DIR}/data/icons/ro.binarylogic.konsole-ssh-manager.svg")
