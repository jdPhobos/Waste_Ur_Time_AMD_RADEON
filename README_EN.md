# Waste Ur Time (AMD ROCm / HIP Edition)

High-speed multi-threaded passcode brute-forcer for **PlayStation 4 (.pkg)** and **PlayStation 5 (.pkg)** package files with hardware acceleration on AMD Radeon graphics cards via the **AMD ROCm / HIP** platform.

The project is built upon the original CPU brute-forcer, featuring a complete overhaul for parallel device-code computations on the GPU. It delivers brute-forcing speeds exceeding **1.1 billion hashes per second (1100+ M/s)** on modern AMD RDNA3 architecture (Radeon RX 7700 XT / 7800 XT / 7900 XT).

---
# Preview

![ezgif com-video-to-gif](https://github.com/HoppersPS4/Waste_Ur_Time/assets/80831610/214df483-16ec-47ba-bc77-0b695cad1843)

## ✨ Features

* **AMD GPU Acceleration:** Full support for computations on AMD Radeon GPUs via the native `clang++` compiler included in the AMD ROCm SDK.
* **Isolated Build Environment:** The compilation shell is protected against conflicts between modern Microsoft STL host libraries (Visual Studio 2022/2026 Preview) and ROCm's internal math parser.
* **Intensity Control (`--intensity`):** Dynamic adjustment of the Batch Size from 1% to 100% directly from the command line. Reduces GPU noise and heat during background brute-forcing.
* **Smart Default:** By default, the GPU runs at a safe intensity of **1%**, preventing instant chip overheating upon accidental execution.
* **Local Duplicate Cache (CPU Mode):** Integration with the high-performance **RocksDB** database via the `vcpkg` package manager to eliminate redundant combination checks on the CPU.
* **Automatic Extraction:** Upon finding the correct password, the utility automatically invokes the official `orbis-pub-cmd.exe` or `prospero-pub-cmd.exe` tools to fully unpack the game contents.

---

## 💻 System Requirements

* **OS:** Windows 10 / 11 (64-bit)
* **GPU:** AMD Radeon (RDNA2/RDNA3 architecture recommended, `gfx1101` cores and similar)
* **Toolchain:**
  * [AMD ROCm SDK v7.2](https://amd.com) for Windows
  * Visual Studio 2022 (with C++ Toolset installed)
  * [CMake v3.23+](https://cmake.org) and [Ninja](https://ninja-build.org) generator
  * Installed and initialized [vcpkg](https://github.com) package manager

---

## 🛠️ Build Instructions
By default, the build targets RX 7700/7800 XT cards. To build for your specific card, change the `--offload-arch=gfx1101` identifier in CMakeLists:
gfx1201	for	RX 9070 XT, RX 9070 GRE, RX 9070
gfx1200	for	RX 9060 XT, RX 9060
gfx1100	for	RX 7900 XTX, RX 7900 XT, RX 7900 GRE
gfx1101	for	RX 7800 XT, RX 7700 XT
gfx1102	for	RX 7600 XT, RX 7600
gfx1030	for	RX 6950 XT, RX 6900 XT, RX 6800 XT, RX 6800
gfx1031	for	RX 6750 XT, RX 6700 XT, RX 6700
gfx1032	for	RX 6650 XT, RX 6600 XT, RX 6600
gfx1034	for	RX 6500 XT, RX 6400

The project must be built strictly using the native `clang++` driver-compiler from the ROCm SDK to ensure seamless linking of host and device code:

```cmd
:: 1. Clear old CMake cache
rmdir /s /q build_amd

:: 2. Configure project in an isolated C++17 HIP environment
cmake -B build_amd -G Ninja ^
-DCMAKE_BUILD_TYPE=Release ^
-DCMAKE_CXX_COMPILER="C:/Program Files/AMD/ROCm/7.2/bin/clang++.exe" ^
-DCMAKE_HIP_COMPILER="C:/Program Files/AMD/ROCm/7.2/bin/clang++.exe" ^
-DCMAKE_HIP_COMPILER_ROCM_ROOT="C:/Program Files/AMD/ROCm/7.2" ^
-DCMAKE_MAKE_PROGRAM=D:/Waste_Ur_Time-main/tools/ninja.exe ^
-DUSE_HIP=ON ^
-DUSE_CUDA=OFF ^
-DCMAKE_TOOLCHAIN_FILE=D:/Waste_Ur_Time-main/vcpkg/scripts/buildsystems/vcpkg.cmake ^
-DVCPKG_TARGET_TRIPLET=x64-windows ^
-DCMAKE_PREFIX_PATH="D:/Waste_Ur_Time-main/vcpkg/installed/x64-windows"

:: 3. Compile and build the executable
cmake --build build_amd
```

*Note: If a Visual Studio STL version conflict occurs during the CMakeTestHIPCompiler test file generation, a forced cache switch `set(CMAKE_HIP_COMPILER_WORKS TRUE CACHE INTERNAL "")` is injected into the `CMakeLists.txt` file before the `enable_language(HIP)` call.*

---

## 🚀 Usage

The executable is compiled into the `build_amd/` directory as `waster_ur_time.exe`.

### Command Line:
```cmd
waster_ur_time.exe <path_to_pkg> <output_folder> [--gpu] [--intensity <1-100>] [--silence] [-t <threads>]
```

### Launch Examples:

1. **Silent background GPU brute-force (1% power by default):**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\UP0001-CUSA00001_00-GAME000000000001.pkg" "D:\Extracted" --gpu
   ```

2. **Maximum GPU brute-force speed (100% power, ~1100+ M/s):**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\patch.pkg" "D:\Extracted" --gpu --intensity 100
   ```

3. **Classic CPU brute-force in 8 threads with RocksDB caching:**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\game.pkg" "D:\Extracted" -t 8
   ```

   *Note: Do not set 100% power unless you are confident your card can handle such loads. Start low and monitor temperatures.*

---

## ⚙️ Arguments Reference

* `<package>` — Required parameter. Path to the target `.pkg` file to crack.
* `<output>` — Required parameter. Folder where `Sc0` and `Image0` files will be extracted after a successful match.
* `--gpu` — Switches the program to AMD HIP hardware acceleration mode. (Supported only for PS4 PKG).
* `--intensity` — Sets the allocated GPU power in the range from `1` to `100`. Directly scales the kernel thread grid size.
* `--silence` — Silent mode. Minimizes console output to save system resources.
* `-t` — Number of concurrent threads to spawn for CPU brute-forcing (in CPU mode).

---

## 🤝 Credits

* **[hoppers](https://github.com/HoppersPS4)** — Project idea, PKG header parsing architecture, and CPU brute-force logic with RocksDB.
* **[Pcniado](https://github.com/Pcniado)** — Development of the SHA-256 math core for GPU.

---

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.
