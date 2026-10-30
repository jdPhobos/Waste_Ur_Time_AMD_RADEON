#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <csignal>
#include <string_view>
#include <rocksdb/db.h>
#include "gpu_bruteforce.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include <random>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <system_error>

std::atomic<bool> passcode_found(false);
std::string found_passcode;
std::string last_used_passcode;
std::string package_name;
std::string package_cid;
std::mutex output_mutex;
bool silence_mode = false;
bool use_gpu = false;
int intensity = 1; // По дефолту минимальная мощность (1%)
volatile std::sig_atomic_t g_signal_received = false;
std::chrono::steady_clock::time_point global_start_time;

std::unique_ptr<rocksdb::DB> db = nullptr;

const std::string ascii_art = R"(
__      __  _____    _______________________________     ____ _____________     ___________.___   _____  ___________
/  \    /  \/  _  \  /   _____/\__    ___/\_   _____/    |    |   \______   \    \__    ___/|   | /     \ \_   _____/
\   \/\/   /  /_\  \ \_____  \   |    |    |    __)_     |    |   /|       _/      |    |   |   |/  \ /  \ |    __)_
\        /    |    \/        \  |    |    |        \    |    |  / |    |   \      |    |   |   /    Y    \|        \
\__/\  /\____|__  /_______  /  |____|   /_______  /    |______/  |____|_  /      |____|   |___\____|__  /_______  /
\/         \/        \/                    \/                      \/                            \/        \/
)";

struct ProcessResult
{
    std::string output = "";
    int returnCode = -1;
};

ProcessResult ExecuteCommand(const std::string& command) {
    ProcessResult result;
#if defined(_WIN32) || defined(_WIN64)
    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE g_hChildStd_OUT_Rd = nullptr, g_hChildStd_OUT_Wr = nullptr;
    HANDLE g_hChildStd_ERR_Rd = nullptr, g_hChildStd_ERR_Wr = nullptr;

    try {
        if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        {
            throw std::runtime_error("Stdout pipe creation failed.");
        }
        if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(g_hChildStd_OUT_Rd);
            CloseHandle(g_hChildStd_OUT_Wr);
            throw std::runtime_error("Stdout SetHandleInformation failed.");
        }
        if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0))
        {
            CloseHandle(g_hChildStd_OUT_Rd);
            CloseHandle(g_hChildStd_OUT_Wr);
            throw std::runtime_error("Stderr pipe creation failed.");
        }
        if (!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(g_hChildStd_OUT_Rd);
            CloseHandle(g_hChildStd_OUT_Wr);
            CloseHandle(g_hChildStd_ERR_Rd);
            throw std::runtime_error("Stderr SetHandleInformation failed.");
        }

        STARTUPINFOA si = {};
        si.cb = sizeof(STARTUPINFOA);
        si.hStdError = g_hChildStd_ERR_Wr;
        si.hStdOutput = g_hChildStd_OUT_Wr;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi = {};

        std::vector<char> commandLine(command.begin(), command.end());
        commandLine.push_back('\0');

        if (!CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi))
        {
            CloseHandle(g_hChildStd_OUT_Rd); CloseHandle(g_hChildStd_OUT_Wr);
            CloseHandle(g_hChildStd_ERR_Rd); CloseHandle(g_hChildStd_ERR_Wr);
            throw std::runtime_error("CreateProcessA failed with error code: " + std::to_string(GetLastError()));
        }

        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(g_hChildStd_ERR_Wr);

        DWORD dwRead;
        const int bufferSize = 4096;
        std::vector<char> buffer(bufferSize);
        std::string output;
        bool bSuccess = TRUE;

        while (bSuccess) {
            bSuccess = ReadFile(g_hChildStd_OUT_Rd, buffer.data(), bufferSize, &dwRead, nullptr);
            if (!bSuccess || dwRead == 0)
                break;
            output.append(buffer.begin(), buffer.begin() + dwRead);
        }
        CloseHandle(g_hChildStd_OUT_Rd);

        bSuccess = TRUE;
        while (bSuccess) {
            bSuccess = ReadFile(g_hChildStd_ERR_Rd, buffer.data(), bufferSize, &dwRead, nullptr);
            if (!bSuccess || dwRead == 0)
                break;
            output.append(buffer.begin(), buffer.begin() + dwRead);
        }
        CloseHandle(g_hChildStd_ERR_Rd);

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&result.returnCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        result.output = output;
    }
    catch (const std::exception& e)
    {
        if (g_hChildStd_OUT_Rd) { CloseHandle(g_hChildStd_OUT_Rd); }
        if (g_hChildStd_OUT_Wr) { CloseHandle(g_hChildStd_OUT_Wr); }
        if (g_hChildStd_ERR_Rd) { CloseHandle(g_hChildStd_ERR_Rd); }
        if (g_hChildStd_ERR_Wr) { CloseHandle(g_hChildStd_ERR_Wr); }
        throw;
    }
#else
    result.output = "";
    result.returnCode = -1;
#endif
    return result;
}
void generate_random_passcode(std::mt19937& gen, std::string& passcode, int length = 32) {
    static constexpr char characters[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";
    static constexpr int char_count = sizeof(characters) - 1;
    std::uniform_int_distribution<int> distribution(0, char_count - 1);

    passcode.resize(length);
    for (int i = 0; i < length; ++i) {
        passcode[i] = characters[distribution(gen)];
    }
}

void ensure_output_directory(const std::string& output_directory) {
    std::filesystem::create_directories(output_directory);
}

static uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

bool parse_pkg_crypto_data(const std::string& pkg_path, PkgCryptoData& out) {
    std::ifstream file(pkg_path, std::ios::binary);
    if (!file) return false;

    char magic[4];
    file.read(magic, 4);
    if (!file || std::memcmp(magic, "\x7F\x43\x4E\x54", 4) != 0) return false;

    uint8_t buf[4];

    file.seekg(0x10);
    file.read(reinterpret_cast<char*>(buf), 4);
    if (!file) return false;
    uint32_t entry_count = read_be32(buf);

    file.seekg(0x18);
    file.read(reinterpret_cast<char*>(buf), 4);
    if (!file) return false;
    uint32_t entry_table_offset = read_be32(buf);

    file.seekg(0x40);
    file.read(out.content_id, 36);
    if (!file) return false;
    out.content_id[36] = '\0';

    file.seekg(entry_table_offset);
    uint32_t keys_data_offset = 0;
    bool found = false;
    for (uint32_t i = 0; i < entry_count && !found; i++) {
        uint8_t entry[32];
        file.read(reinterpret_cast<char*>(entry), 32);
        if (!file) return false;
        uint32_t id          = read_be32(entry);
        uint32_t data_offset = read_be32(entry + 16);
        if (id == 0x0010) {
            keys_data_offset = data_offset;
            found = true;
        }
    }

    if (!found) {
        std::cerr << "[-] ENTRY_KEYS not found in PKG entry table." << std::endl;
        return false;
    }

    file.seekg(keys_data_offset + 32);
    file.read(reinterpret_cast<char*>(out.expected_digest), 32);
    if (!file) return false;

    out.valid = true;
    return true;
}

bool is_pkg_file(const std::string& file_name) {
    return file_name.size() >= 4 && file_name.substr(file_name.size() - 4) == ".pkg";
}

std::string read_cid(const std::string& package_file) {
    std::ifstream file(package_file, std::ios::binary);
    if (!file) {
        std::cerr << "[-] Failed to open the package file." << std::endl;
        return "";
    }

    file.seekg(0x40, std::ios::beg);
    char cid_buffer[36];
    file.read(cid_buffer, sizeof(cid_buffer));
    if (!file) {
        std::cerr << "[-] Failed to read the CID from the package file." << std::endl;
        return "";
    }

    std::string cid_string(cid_buffer, sizeof(cid_buffer));
    cid_string.erase(std::remove_if(cid_string.begin(), cid_string.end(),
        [](unsigned char c) { return !std::isprint(c); }), cid_string.end());
    return cid_string;
}

bool CheckExecutable(const std::string& executableName) {
#if defined(_WIN32) || defined(_WIN64)
    std::string path = ".";
    std::string fullPath = path + "\\" + executableName;
    return std::filesystem::exists(fullPath);
#else
    return true;
#endif
}

bool already_tried(const std::string &code, std::string *output) {
    static const rocksdb::ReadOptions ro;
    static const rocksdb::WriteOptions wo;
    auto s = db->Get(ro, code, output);
    if (s.ok())
        return true;
    db->Put(wo, code, "0");
    return false;
}

void brute_force_passcode_thread(const std::string& input_file, const std::string& output_directory,
                                 const std::string& command_prefix, bool is_ps5, std::mt19937 gen) {
    const std::string Sc0Path = output_directory + "/Sc0";
    const std::string Image0Path = output_directory + "/Image0";
    const std::string input_quoted = "\"" + input_file + "\"";
    const std::string output_quoted = "\"" + output_directory + "\"";

    std::string display_label;
    if (!is_ps5) {
        display_label = package_cid;
    } else {
        display_label = std::filesystem::path(input_file).stem().string();
    }

    std::string passcode;
    passcode.reserve(32);

    std::string full_command;
    full_command.reserve(command_prefix.size() + 32 + input_quoted.size() + output_quoted.size() + 4);

    std::string tested;
    uint64_t attempt_count = 0;

    while (!passcode_found.load(std::memory_order_relaxed)) {
        generate_random_passcode(gen, passcode);
        ++attempt_count;

        if ((attempt_count % 500) == 0) {
            tested.clear();
            if (already_tried(passcode, &tested)) {
                if (tested == "1") {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    passcode_found.store(true, std::memory_order_release);
                    found_passcode = passcode;
                    last_used_passcode = passcode;
                    std::cout << "Congratulations, we did all this work to repeat the right one: " << passcode << std::endl;
                    break;
                }
                continue;
            }
        }

        full_command.clear();
        full_command.append(command_prefix);
        full_command.append(passcode);
        full_command.push_back(' ');
        full_command.append(input_quoted);
        full_command.push_back(' ');
        full_command.append(output_quoted);

        ProcessResult result;
        try {
            result = ExecuteCommand(full_command);
        }
        catch (const std::exception& e) {
            continue;
        }

        if (!silence_mode && (attempt_count % 50) == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - global_start_time).count();
            auto hours = elapsed_seconds / 3600;
            auto minutes = (elapsed_seconds % 3600) / 60;
            auto seconds = elapsed_seconds % 60;

            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "[" << std::setw(2) << std::setfill('0') << hours << "h "
                      << std::setw(2) << std::setfill('0') << minutes << "m "
                      << std::setw(2) << std::setfill('0') << seconds << "s] | Passcode: " << passcode
                      << " | " << display_label << std::endl;
        }

        if (result.returnCode == 0 && std::filesystem::exists(Sc0Path) && std::filesystem::exists(Image0Path)) {
            passcode_found.store(true, std::memory_order_release);
            found_passcode = passcode;
            db->Put(rocksdb::WriteOptions(), passcode, "1");
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(output_mutex);
        last_used_passcode = passcode;
    }
}
void brute_force_passcode(const std::string& input_file, const std::string& output_directory, int num_threads) {
    ensure_output_directory(output_directory);
    global_start_time = std::chrono::steady_clock::now();

    const char ps4_header_magic[] = { 0x7F, 0x43, 0x4E, 0x54 };
    const char ps5_header_magic[] = { 0x7F, 0x46, 0x49, 0x48 };

    char header[4];
    std::ifstream file(input_file, std::ios::binary);
    if (!file.read(header, sizeof(header))) {
        std::cerr << "[-] Error reading from the file." << std::endl;
        return;
    }
    file.close();

    std::string command;
    bool is_ps5 = false;

    if (std::memcmp(header, ps4_header_magic, sizeof(ps4_header_magic)) == 0) {
        std::cout << "[+] Detected PS4 package file." << std::endl;
        if (!CheckExecutable("orbis-pub-cmd.exe")) {
            std::cerr << "[-] Required executable 'orbis-pub-cmd.exe' not found." << std::endl;
            return;
        }
        command = "orbis-pub-cmd.exe img_extract --passcode ";
        package_cid = read_cid(input_file);
    }
    else if (std::memcmp(header, ps5_header_magic, sizeof(ps5_header_magic)) == 0) {
        std::cout << "[+] Detected PS5 package file." << std::endl;
        if (!CheckExecutable("prospero-pub-cmd.exe")) {
            std::cerr << "[-] Required executable 'prospero-pub-cmd.exe' not found." << std::endl;
            return;
        }
        command = "prospero-pub-cmd.exe img_extract --passcode ";
        is_ps5 = true;
    }
    else {
        std::cerr << "[-] Invalid package file header." << std::endl;
        return;
    }

    std::random_device rd;
    std::vector<std::thread> threads;

    if (num_threads == 0)
        num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
        num_threads = 4;

    std::cout << "[+] Running with " << num_threads << " threads" << std::endl;

    for (int i = 0; i < num_threads; ++i) {
        std::mt19937 gen(rd());
        threads.emplace_back(brute_force_passcode_thread, input_file, output_directory, command, is_ps5, gen);
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

void brute_force_passcode_gpu(const std::string& input_file, const std::string& output_directory) {
    ensure_output_directory(output_directory);
    global_start_time = std::chrono::steady_clock::now();

    const char ps4_magic[] = { 0x7F, 0x43, 0x4E, 0x54 };
    char header[4];
    {
        std::ifstream file(input_file, std::ios::binary);
        if (!file.read(header, 4)) {
            std::cerr << "[-] Error reading the file." << std::endl;
            return;
        }
    }
    if (std::memcmp(header, ps4_magic, 4) != 0) {
        std::cerr << "[-] GPU mode only supports PS4 PKG files." << std::endl;
        return;
    }

    std::cout << "[+] Detected PS4 package file." << std::endl;

    int ngpus = gpu_device_count();
    std::cout << "[+] GPUs detected: " << ngpus << std::endl;
    for (int i = 0; i < ngpus; i++)
        std::cout << "[+]   GPU " << i << ": " << gpu_device_name(i) << std::endl;

    PkgCryptoData crypto{};
    if (!parse_pkg_crypto_data(input_file, crypto)) {
        std::cerr << "[-] Failed to parse PKG crypto data." << std::endl;
        return;
    }

    std::cout << "[+] Running in GPU mode with intensity: " << intensity << "%" << std::endl;

    package_cid = std::string(crypto.content_id);
    std::cout << "[+] Content ID: " << package_cid << std::endl;

    std::cout << "[+] Keys.digest: ";
    for (int i = 0; i < 32; i++)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)crypto.expected_digest[i];
    std::cout << std::dec << std::endl;

    std::string result = gpu_brute_force(crypto, passcode_found, silence_mode, intensity);

    if (!result.empty()) {
        found_passcode = result;

        if (CheckExecutable("orbis-pub-cmd.exe")) {
            std::cout << "[+] Verifying passcode and extracting PKG..." << std::endl;
            std::string cmd = "orbis-pub-cmd.exe img_extract --passcode " + result
                            + " \"" + input_file + "\" \"" + output_directory + "\"";
            ProcessResult pr = ExecuteCommand(cmd);
            if (pr.returnCode == 0) {
                std::cout << "[+] PKG extracted successfully!" << std::endl;
            } else {
                std::cout << "[!] Extraction tool returned code " << pr.returnCode
                          << ". You may need to extract manually with the found passcode."
                          << std::endl;
            }
        } else {
            std::cout << "[!] orbis-pub-cmd.exe not found. Run extraction manually with passcode: "
                      << result << std::endl;
        }
    }
}

void SignalHandler(int signal) {
    g_signal_received = true;
    if (!last_used_passcode.empty()) {
        std::cout << "[+] Last used passcode: " << last_used_passcode << std::endl;
    }
    std::cout << "[+] Exiting..." << std::endl;
    if (db) db.reset();
    exit(signal);
}

void printHelp(char *argv[]) {
    std::cerr << "\nUsage: " << argv[0]
              << " <package> <output> [--silence] [--gpu] [--intensity <1-100>] [-t <threads>]\n"
              << "<package> - The package file to brute force.\n"
              << "<output> - Output directory.\n"
              << "--silence   - Activates 'Silence Mode' for minimal output.\n"
              << "--gpu       - Use GPU acceleration (CUDA/HIP, PS4 PKGs only).\n"
              << "--intensity - GPU power intensity percent from 1 to 100 (Default: 100).\n"
              << "-t <threads> - Sets the number of threads for CPU mode (Default: hardware concurrency).\n";
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned>(time(nullptr)));
    int num_threads = 0;

    if (argc < 3 || argc > 9) {
        printHelp(argv);
        return 1;
    }

#if defined(_WIN32) || defined(_WIN64)
    std::system("cls");
#endif

    std::cout << ascii_art << std::endl;

    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") {
            printHelp(argv);
            return 1;
        } else if (std::string(argv[i]) == "--silence") {
            silence_mode = true;
        } else if (std::string(argv[i]) == "--gpu") {
            use_gpu = true;
        } else if (std::string(argv[i]) == "--intensity" && i + 1 < argc) {
            try {
                intensity = std::stoi(argv[i + 1]);
                if (intensity < 1 || intensity > 100) {
                    std::cerr << "[-] Invalid intensity: Must be between 1 and 100." << std::endl;
                    return 1;
                }
                i++;
            } catch (...) {
                std::cerr << "[-] Invalid intensity value." << std::endl;
                return 1;
            }
        } else if (std::string(argv[i]) == "-t" && i + 1 < argc) {
            try {
                num_threads = std::stoi(argv[i + 1]);
                if (num_threads < 0) {
                    std::cerr << "[-] Invalid thread count: Must be a non-negative number." << std::endl;
                    return 1;
                }
                i++;
            }
            catch (...) {
                std::cerr << "[-] Invalid thread count." << std::endl;
                return 1;
            }
        }
    }

    if (silence_mode) {
        std::cout << "[+] Silence Mode activated. This Window will be quiet..." << std::endl;
    }
    else {
        std::cout << "Made by hoppers, GPU support added by Pcniado" << std::endl;
    }

    package_name = argv[1];
    std::string output = argv[2];

    if (!is_pkg_file(package_name)) {
        std::cerr << "[-] Invalid package file format." << std::endl;
        return 1;
    }

    if (use_gpu) {
        if (!gpu_available()) {
            std::cerr << "[-] No GPU detected. Falling back to CPU mode." << std::endl;
            use_gpu = false;
        }
    }

    if (!use_gpu) {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction(512 << 20);
        options.write_buffer_size = 128 << 20;
        options.max_write_buffer_number = 4;
        options.min_write_buffer_number_to_merge = 2;

        auto status = rocksdb::DB::Open(options, "progress_db", &db);
        if (!status.ok()) {
            std::cerr << "[-] Somehow, failed to open the database, is it corrupted?" << std::endl;
            return 1;
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    std::string windowTitle = "Waste Ur Time is wasting ur time on " + package_name;
    SetConsoleTitleA(windowTitle.c_str());
#endif

    std::signal(SIGINT, SignalHandler);

    if (use_gpu) {
        brute_force_passcode_gpu(package_name, output);
    } else {
        brute_force_passcode(package_name, output, num_threads);
    }

    if (passcode_found) {
        std::string successFileName = package_name + ".success";
        std::ofstream successFile(successFileName);
        if (successFile.is_open()) {
            successFile << found_passcode;
            successFile.close();
            std::cout << "[+] Passcode has been saved to: " << successFileName << std::endl;
        }
        else {
            std::cerr << "[-] Failed to create/save the success file." << std::endl;
        }
        std::cout << "[+] We did it!\n[+] Passcode: " << found_passcode << std::endl;
    }
    else {
        std::cout << "[-] Passcode not found." << std::endl;
    }

    if (db) db.reset();
    return 0;
}
