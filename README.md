> [!TIP]
> [Description in English](README_EN.md)
# Waste Ur Time (AMD ROCm / HIP Edition)

Высокоскоростной многопоточный брутфорсер кодов доступа (passcode) для файлов пакетов **PlayStation 4 (.pkg)** и **PlayStation 5 (.pkg)** с аппаратным ускорением на видеокартах AMD Radeon через платформу **AMD ROCm / HIP**.

Проект разработан на базе оригинального CPU-брутфорсера с полной переработкой под параллельные вычисления девайс-кода на GPU. Обеспечивает скорость перебора более **1.1 миллиарда хэшей в секунду (1100+ M/s)** на современной архитектуре AMD RDNA3 (Radeon RX 7700 XT / 7800 XT / 7900 XT).

---
# Preview

![ezgif com-video-to-gif](https://github.com/HoppersPS4/Waste_Ur_Time/assets/80831610/214df483-16ec-47ba-bc77-0b695cad1843)

## ✨ Особенности (Features)

* **AMD GPU Acceleration:** Полноценная поддержка вычислений на GPU AMD Radeon через нативный компилятор `clang++` из состава AMD ROCm SDK.
* **Изолированное окружение сборки:** Оболочка компиляции защищена от конфликтов современных хост-библиотек Microsoft STL (Visual Studio 2022/2026 Preview) с внутренним математическим парсером ROCm.
* **Управление интенсивностью (`--intensity`):** Динамическое регулирование размера батча (Batch Size) от 1% до 100% прямо из командной строки. Снижает шум и нагрев видеокарты при фоновом переборе.
* **Умный дефолт:** По умолчанию GPU запускается на безопасной интенсивности в **1%**, предотвращая мгновенный перегрев чипа при случайном запуске.
* **Локальный кэш дубликатов (в CPU-режиме):** Интеграция с высокопроизводительной СУБД **RocksDB** через менеджер пакетов `vcpkg` для исключения повторной проверки комбинаций на процессоре.
* **Автоматическая экстракция:** При нахождении верного пароля утилита автоматически вызывает официальные инструменты `orbis-pub-cmd.exe` или `prospero-pub-cmd.exe` для полной распаковки содержимого игры.

---

## 💻 Системные требования (Requirements)

* **ОС:** Windows 10 / 11 (64-bit)
* **Видеокарта:** AMD Radeon (рекомендуется архитектура RDNA2/RDNA3, ядра `gfx1101` и аналогичные)
* **Инструментарий:**
  * [AMD ROCm SDK v7.2](https://amd.com) для Windows
  * Visual Studio 2022 (с установленным Toolset C++)
  * [CMake v3.23+](https://cmake.org) и генератор [Ninja](https://ninja-build.org)
  * Установленный и инициализированный менеджер пакетов [vcpkg](https://github.com)

---

## 🛠️ Инструкция по сборке (Compilation)
По умолчанию сборка происходит для карт RX 7700\7800 XT. Для сборки под вашу карту, измените в CMakeLists идентификатор карты --offload-arch=gfx1101:
gfx1201	для	RX 9070 XT, RX 9070 GRE, RX 9070
gfx1200	для	RX 9060 XT, RX 9060
gfx1100	для	RX 7900 XTX, RX 7900 XT, RX 7900 GRE
gfx1101	для	RX 7800 XT, RX 7700 XT
gfx1102	для	RX 7600 XT, RX 7600
gfx1030	для	RX 6950 XT, RX 6900 XT, RX 6800 XT, RX 6800
gfx1031	для	RX 6750 XT, RX 6700 XT, RX 6700
gfx1032	для	RX 6650 XT, RX 6600 XT, RX 6600
gfx1034	для	RX 6500 XT, RX 6400

Сборка проекта осуществляется строго через нативный драйвер-компилятор `clang++` из состава ROCm SDK, чтобы обеспечить бесшовную линковку хост- и девайс-кода:

```cmd
:: 1. Очистка старого кэша CMake
rmdir /s /q build_amd

:: 2. Конфигурация проекта в изолированном C++17 HIP-окружении
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

:: 3. Компиляция и сборка исполняемого файла
cmake --build build_amd
```

*Примечание: Если во время генерации тестового файла CMakeTestHIPCompiler возникает конфликт версий STL Visual Studio, в тело файла `CMakeLists.txt` перед вызовом `enable_language(HIP)` внедряется принудительный переключатель кэша `set(CMAKE_HIP_COMPILER_WORKS TRUE CACHE INTERNAL "")`.*

---

## 🚀 Использование (Usage)

Исполняемый файл компилируется в каталог `build_amd/waster_ur_time.exe`.

### Командная строка:
```cmd
waster_ur_time.exe <путь_к_pkg> <папка_вывода> [--gpu] [--intensity <1-100>] [--silence] [-t <потоки>]
```

### Примеры запуска:

1. **Фоновый бесшумный перебор на GPU (по умолчанию 1% мощности):**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\UP0001-CUSA00001_00-GAME000000000001.pkg" "D:\Extracted" --gpu
   ```

2. **Максимальная скорость перебора на GPU (100% мощности, ~1100+ M/s):**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\patch.pkg" "D:\Extracted" --gpu --intensity 100
   ```

3. **Классический перебор на процессоре (CPU) в 8 потоков с кэшированием в RocksDB:**
   ```cmd
   build_amd\waster_ur_time.exe "D:\Games\game.pkg" "D:\Extracted" -t 8
   ```

   *Примечание: не выставляйте 100% мощности, если не уверены в том, что ваша карта способна работать в таком режиме. Начните с малого и мониторьте температуру.*

---

## ⚙️ Синтаксис флагов (Arguments Reference)

* `<package>` — Обязательный параметр. Путь к целевому файлу `.pkg` для взлома.
* `<output>` — Обязательный параметр. Папка, куда будут извлечены файлы `Sc0` и `Image0` после успешного подбора.
* `--gpu` — Переключает программу в режим аппаратного ускорения AMD HIP. (Поддерживается только для PS4 PKG).
* `--intensity` — Задает выделяемую мощность на GPU в диапазоне от `1` до `100`. Напрямую масштабирует размер сетки потоков ядра.
* `--silence` — Режим тишины. Минимизирует вывод в консоль для экономии системных ресурсов.
* `-t` — Количество одновременно создаваемых потоков для перебора на центральном процессоре (в CPU-режиме).

---

## 🤝 Авторы и благодарности (Credits)

* **[hoppers](https://github.com/HoppersPS4)** — Идея проекта, архитектура парсинга заголовков PKG и логика CPU-перебора с RocksDB.
* **[Pcniado](https://github.com/Pcniado)** — Разработка математического ядра SHA-256 под GPU.

---

## 📄 Лицензия (License)

This project is licensed under the MIT License - see the LICENSE file for details.
