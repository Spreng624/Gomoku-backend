@echo off
cd build
cmake -G "MinGW Makefiles" ..
cmake --build . --target GomokuBackend_test
ctest --output-on-failure
pause
