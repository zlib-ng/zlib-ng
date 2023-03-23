# zlib-ng for RISC-V

## Update Submodules

```
# (In zlib-ng folder)
./script/update_submodules.sh
```

## Download RISC-V Toolchain

### (Optional) Setup SSH Key
The sifive prebuilt toolchains and QEMU are prepared on sifive server through NFS. If you **are not** using sifive server, our bootstrap script will use `scp` to download the files through `login2.sifive.com`. So, you should setup your ssh key for the sifive login2 server. Here is the example:

```
(in ~/.ssh/config)
Host login2.sifive.com # The login2 server dns name
User <your-user-id>
ForwardAgent yes
IdentityFile ~/.ssh/<your-sifive-ssh-key> # The path for your `private` key
```

### Install RISC-V Toolchain

```
# (In zlib-ng folder)
# Install riscv linux toolchains.
./script/riscv/bootstrap.sh
```

### (Optional) Setup Environment on SiFive Server
If you try to build zlib-ng on sifive server, you will need to setup modules.

```
source ./script/setup_modules.sh
```

## Setup Maximum Number of Concurrent Build Processes
We could setup the `CMAKE_BUILD_PARALLEL_LEVEL` environment variable for the maximum process number.

## Build zlib-ng for RISC-V QEMU/Linux

```
# (In zlib-ng folder)
cmake -G Ninja -B ./build-riscv -D CMAKE_TOOLCHAIN_FILE=./cmake/toolchain-riscv.cmake -D CMAKE_INSTALL_PREFIX=./build-riscv/install .

# Use 8 process number for building
export CMAKE_BUILD_PARALLEL_LEVEL=8
cmake --build ./build-riscv --target install
```

By default, the unit tests are built as `gtest_zlib`, and installed to the path.

Run the tests:
```
./script/riscv/qemu_run.sh <path_of_gtest_bin>
```

e.g.
```
./script/riscv/qemu_run.sh build-riscv/gtest_zlib
```

## Build benchmark for RISC-V QEMU/Linux

We recommend using static linking.
```
cmake -G Ninja -B ./build-riscv \
  -D CMAKE_TOOLCHAIN_FILE=./cmake/toolchain-riscv.cmake \
  -D CMAKE_INSTALL_PREFIX=./build-riscv/install \
  -D WITH_BENCHMARKS=ON \
  -D CMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
  -D CMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
  -D CMAKE_MODULE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
  .

cmake --build ./build-riscv --target install
```

The benchmarking program will be saved at `<build_directory>/test/benchmarks/benchmark_zlib`.
