@echo off
cd build
cmake -G "MinGW Makefiles" ..
cmake --build . --target GomokuBackend_tests
ctest --output-on-failure
pause
