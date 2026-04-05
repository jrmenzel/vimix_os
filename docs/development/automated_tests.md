# Automated Testing

## Clang-Format

The source formatting can be checked via clang-format with `tools/tests/check_formatting.sh`. Note that clang 20 and newer disagree on the formatting with the projects `.clang-format` file to clang 18.

## Test apps

The OS can be tested with some special user space applications, most notably [usertests](../userspace/tests/usertests.md) (more are listed in [user space](../userspace/userspace.md)). These test various [kernel APIs](../kernel/syscalls/syscalls.md) and std C lib functions.

## (Semi) Automated Testing

To automate tests run in the [user space](../userspace/userspace.md) of VIMIX, a special shell script can be run automatically after booting. It works like this:

- [init](../userspace/bin/init.md) starts [login](../userspace/bin/login.md)
- [login](../userspace/bin/login.md) is per default configured to log in user `root` without authentifizieren (which also makes interactive testing easier).
	- It is also set up to start the [shell](../userspace/bin/sh.md)  with the command line argument `/autoexec.sh` if such a file is present. See `root/etc/login.conf`.

So to auto start a test, a shell script must be added to the [file system](../kernel/file_system/file_system.md) image after build.

```bash
make
./tools/set_autoexec.sh root/tests/usertests.sh
make qemu-run
```

**Note:** `make qemu-run` does not re-create the FS image, but `make qemu` would!

Hints for test automation scripts:
- The script should end with `shutdown -h` to end `qemu`/`spike`. 
	- The emulator will quit with a success code (even after a shutdown due to a kernel panic), so the output must be redirected to a file and get parsed to determine a positive test result. E.g. by looking for `ALL TESTS PASSED` from [usertests](../userspace/tests/usertests.md).
- The kernel should be [compiled](build_instructions.md) with `EXTERNAL_KERNEL_FLAGS=-D_SHUTDOWN_ON_PANIC` which will prevent the emulator being stuck in a kernel panic and never ending the test (in most cases at least).
- See existing tests in `root/tests/`. 

The host script `tools/run_tests.sh` can compile for different [platforms/architectures](../misc/architectures.md) and run various tests. 

## CI/CD

This repository includes configs and scripts for automated testing on a [private GitLab](cicd.md) instance. They wont be useful for anyone else as is.


---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) 
