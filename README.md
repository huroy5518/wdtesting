# Wdtesting: A Kernel Module Testing Framework

## Overview

Wdtesting is a testing framework designed for Linux kernel modules. It provides a comprehensive solution for writing, running, and analyzing unit and integration tests for kernel code. The framework leverages a combination of a user-space controller, a kernel-space test runner, and a set of powerful code instrumentation tools.

The core features of Wdtesting include:
- **Test Case Management**: A simple API to define and register test cases.
- **Assertions**: A collection of assertion macros to verify test results.
- **Mocking**: A mechanism to mock functions, allowing for isolated unit testing.
- **Code Coverage**: Automatic code instrumentation to provide detailed code coverage analysis.
- **Debugfs Interface**: A user-friendly interface via debugfs to control test execution and gather results from user-space.

## Directory Structure

```
├───_test/              # Directory for test-specific files and build artifacts.
├───instrument-tool/    # Source code for the instrumentation tools.
├───script/             # Build and helper scripts.
├───src/                # Source code of the testing framework.
├───Makefile            # Main Makefile for the project.
├───README.md           # This file.
```

## Build Process

The project uses a multi-stage build process orchestrated by the main `Makefile`.

1.  **Tool Compilation**: The `make tool` command compiles the instrumentation tools located in the `instrument-tool/` directory. These tools are essential for the code coverage and mocking features.

2.  **Pre-compilation Stage**: The `make pre` command prepares the source code for testing. This stage involves several steps:
    -   It uses `bear` to generate a compilation database (`compile_commands.json`), which is then parsed to extract compiler flags.
    -   It runs the `pre-stage.sh` script, which uses the instrumentation tools to instrument the source code. The instrumented code is placed in the `_test/` directory.
    -   It prepares the kernel module build environment.
    -   It builds the test module in the `_test/` directory.

3.  **Compilation**: The `make compile` command is intended for the main compilation of the project, but it is currently not fully implemented.

4.  **Post-compilation Stage**: The `make post` command runs the `post-stage.sh` script, which can be used for cleanup or other post-build tasks.

To build the entire project, simply run `make`.

## Testing Framework

The testing framework is implemented in `src/wdtest_impl.c` and `src/wdtest_debugfs.c`.

### Writing Tests

Test cases are defined as functions with the `test_fp` signature. The `add_test` function is used to register a test case with the framework.

```c
// Example test case
int my_test_function(void) {
    // Test logic here
    int result = some_function_to_test();
    ASSERT_i64_EQ(result, 0, TEST_INFO);
    return 0;
}

// Register the test case in TEST_CASES
void TEST_CASES(void) {
    TEST("My Test Case", my_test_function);
}
```

### Running Tests

The tests are controlled through a debugfs interface. Once the test module is loaded, you can trigger the tests by writing to the `trigger_test` file in the `/sys/kernel/debug/wdtest/` directory.

```sh
# Trigger the tests
echo 1 > /sys/kernel/debug/wdtest/trigger_test
```

### Gathering Results

The test results, including coverage data, can be gathered by reading the `gather_test_result` file.

```sh
# Gather test results
cat /sys/kernel/debug/wdtest/gather_test_result
```

The output will be a CSV-formatted list of block IDs, begin counts, and end counts, which can be used for coverage analysis. The `line_coverage.py` script can be used to parse this data and generate a coverage report.

## Instrumentation Tools

The `instrument-tool/` directory contains a set of Clang-based tools for C code instrumentation.

-   **`instrument`**: This is the main tool for code coverage. It traverses the AST of the source code and injects `beginning_func` and `end_func` calls at the beginning and end of each function and block. This allows the framework to track which parts of the code are executed during a test run.
-   **`expose_function`**: A tool to expose static functions, making them available for testing.
-   **`generate_header`**: A tool to generate header files.
-   **`mock`**: A tool for mocking functions.
-   **`module_init_macro`**: A tool for handling module initialization macros.
-   **`remove_static`**: A tool to remove the `static` keyword from functions and variables.

## Usage
The expected folder structure for using this framework is like the following:
```
├───<src_of_your_module>
    ├───Makefile
    ├───wdtesting/
```
And you need to modify your Makefile like the following:
```
# old Makefile:
.PHONY: all clean test
ARCH ?= arm64

# 2. Specify the Compiler Prefix
# Ensure this is in your $PATH, or provide the absolute path like /usr/bin/aarch64-linux-gnu-
CROSS_COMPILE ?=aarch64-linux-gnu-

# 3. Specify the Path to the Kernel Source
# This must point to your prepared Linux 6.1.31 folder
KDIR?=$(realpath ../linux-6.1.31)

# SRCS:=pci_debugfs.c
SRCS:=main.c example.c

# Module Name
obj-m+=pci_debugfs.o

pci_debugfs-y+=example.o main.o $(TEST_OBJS)
# pci_debugfs-y+=$(TEST_OBJS)

ccflags-y+=-I$(TEST_INCLUDE)

all:
# 	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules_prepares
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean



# modified Makefile:
.PHONY: all clean test

ARCH ?= arm64

TEST_INCLUDE ?=.

# 2. Specify the Compiler Prefix
# Ensure this is in your $PATH, or provide the absolute path like /usr/bin/aarch64-linux-gnu-
CROSS_COMPILE ?=aarch64-linux-gnu-

# 3. Specify the Path to the Kernel Source
# This must point to your prepared Linux 6.1.31 folder
KDIR?=$(realpath ../linux-6.1.31)

# SRCS:=pci_debugfs.c
SRCS:=main.c example.c

TEST_FILES?=$(realpath ./pci_test.c)

# Module Name
obj-m+=pci_debugfs.o

TEST_OBJS?=
pci_debugfs-y+=example.o main.o $(TEST_OBJS)
# pci_debugfs-y+=$(TEST_OBJS)

ccflags-y+=-I$(TEST_INCLUDE)

all:
# 	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules_prepares
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	
test:
	make -C wdtesting TEST_FILES="$(TEST_FILES)" KDIR=$(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) SRCS="$(SRCS)"  all

clean:
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

diff:
```
1a2
> 
3a5,6
> TEST_INCLUDE ?=.
> 
14a18,20
> TEST_FILES?=$(realpath ./pci_test.c)
> 
> 
17a24
> TEST_OBJS?=
25a33,35
> 
> test:
>       make -C wdtesting TEST_FILES="$(TEST_FILES)" KDIR=$(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) SRCS="$(SRCS)"  all
```