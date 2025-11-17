# Run the tests

Before starting the `test_all.sh` script make sure that:
- `mini-sandbox` and `mini-tapbox` are in the PATH 
- the linker can find `libminisandbox.so` and `libminitapbox.so` (easy way is to set LD_LIBRARY_PATH)

The simpler way is to run `build.sh --install` and that will take care of install minisandbox system-wide . 
Otherwise, you can just manually set the env variable PATH/LD_LIBRARY_PATH

Then just run `test_all.sh`
