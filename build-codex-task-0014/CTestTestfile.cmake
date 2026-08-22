# CMake generated Testfile for 
# Source directory: /mnt/F/Arch Dock
# Build directory: /mnt/F/Arch Dock/build-codex-task-0014
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("panel-registry-test" "/mnt/F/Arch Dock/build-codex-task-0014/panel-registry-test")
set_tests_properties("panel-registry-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=/mnt/F/Arch Dock/build-codex-task-0014/test-data/panel-registry" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;122;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("plasma-script-result-test" "/mnt/F/Arch Dock/build-codex-task-0014/plasma-script-result-test")
set_tests_properties("plasma-script-result-test" PROPERTIES  _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;140;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("dock-geometry-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_DockGeometry.qml")
set_tests_properties("dock-geometry-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;144;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("motion-policy-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_MotionPolicy.qml")
set_tests_properties("motion-policy-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;152;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("free-entry-policy-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_FreeEntryPolicy.qml")
set_tests_properties("free-entry-policy-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;160;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("bootstrap-coordinator-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_BootstrapCoordinator.qml")
set_tests_properties("bootstrap-coordinator-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;168;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("plasma-template-contract-test" "/usr/bin/cmake" "-DSOURCE_DIR=/mnt/F/Arch Dock" "-P" "/mnt/F/Arch Dock/tests/ValidatePlasmaTemplate.cmake")
set_tests_properties("plasma-template-contract-test" PROPERTIES  _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;176;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("studio-navigation-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_StudioNavigation.qml")
set_tests_properties("studio-navigation-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;182;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
add_test("studio-draft-test" "/usr/lib/qt6/bin/qmltestrunner" "-input" "/mnt/F/Arch Dock/tests/tst_StudioDraft.qml")
set_tests_properties("studio-draft-test" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/mnt/F/Arch Dock/CMakeLists.txt;190;add_test;/mnt/F/Arch Dock/CMakeLists.txt;0;")
