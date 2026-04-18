#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <map>
#include <functional>
#include <array>
#include <fstream>
#include <dirent.h>
#include <sys/sysinfo.h>

using namespace std;
using namespace chrono;

#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define BOLD_RED    "\033[1;31m"
#define BOLD_GREEN  "\033[1;32m"
#define BOLD_YELLOW "\033[1;33m"
#define BOLD_BLUE   "\033[1;34m"
#define BOLD_CYAN   "\033[1;36m"
#define BOLD_WHITE  "\033[1;37m"
#define BG_BLUE     "\033[44m"
#define BG_CYAN     "\033[46m"

volatile sig_atomic_t g_running = 1;

void signalHandler(int signum) {
    g_running = 0;
}

struct CPUStats {
    long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    CPUStats() : user(0), nice(0), system(0), idle(0), iowait(0), irq(0), softirq(0), steal(0), guest(0), guest_nice(0) {}
};

struct CPUCore {
    int id;
    double usage;
    long long frequency;
    double temperature;
    string governor;
};

struct MemoryInfo {
    long long total, available, used, buffers, cached, swapTotal, swapFree;
    double usagePercent;
};

struct BatteryInfo {
    long current;
    int capacity;
    int voltage;
    string status;
    string health;
    double temperature;
    bool isCharging;
};

struct NetworkInfo {
    long long rxBytes, txBytes;
    long long rxSpeed, txSpeed;
};

struct ProcessInfo {
    int pid;
    string name;
    double cpuPercent;
    long long memory;
    string user;
};

class TerminalUtils {
public:
    static void clearScreen() {
        cout << "\033[2J\033[1;1H";
    }
    
    static void hideCursor() {
        cout << "\033[?25l";
    }
    
    static void showCursor() {
        cout << "\033[?25h";
    }
    
    static void setCursorPosition(int x, int y) {
        cout << "\033[" << y << ";" << x << "H";
    }
    
    static pair<int, int> getTerminalSize() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return {w.ws_col, w.ws_row};
    }
    
    static string colorize(double value, double warn, double crit, const string& unit = "%") {
        if (value >= crit) return BOLD_RED + to_string((int)value) + unit + RESET;
        if (value >= warn) return BOLD_YELLOW + to_string((int)value) + unit + RESET;
        return GREEN + to_string((int)value) + unit + RESET;
    }
    
    static string colorizeTemp(double temp) {
        if (temp >= 60) return BOLD_RED;
        if (temp >= 50) return BOLD_YELLOW;
        if (temp >= 40) return YELLOW;
        return GREEN;
    }
};

class ProgressBar {
public:
    static string generate(double percentage, int width = 20, bool showPercent = true) {
        string bar = "[";
        int pos = (int)(width * (percentage / 100.0));
        
        for (int i = 0; i < width; ++i) {
            if (i < pos) {
                if (percentage >= 90) bar += RED "#";
                else if (percentage >= 75) bar += YELLOW "#";
                else if (percentage >= 50) bar += BLUE "#";
                else bar += GREEN "#";
            } else {
                bar += DIM "-";
            }
        }
        bar += RESET "]";
        
        if (showPercent) {
            stringstream ss;
            ss << " " << fixed << setprecision(1) << setw(5) << percentage << "%";
            bar += ss.str();
        }
        
        return bar;
    }
    
    static string generateGradient(double percentage, int width = 20) {
        string bar = "[";
        int pos = (int)(width * (percentage / 100.0));
        
        for (int i = 0; i < width; ++i) {
            if (i < pos) {
                int colorCode = 40 + (int)(i * 1.5);
                bar += "\033[38;5;" + to_string(colorCode) + "m█" RESET;
            } else {
                bar += DIM "░" RESET;
            }
        }
        
        stringstream ss;
        bar += RESET "] " + to_string((int)percentage) + "%";
        return bar;
    }
};

class CPUMonitor {
private:
    vector<CPUStats> prevStats;
    vector<long long> prevFreqs;
    vector<double> prevTemps;
    int coreCount;
    
    long long getCoreFrequency(int core) {
        string path = "/sys/devices/system/cpu/cpu" + to_string(core) + "/cpufreq/scaling_cur_freq";
        ifstream file(path);
        long long freq;
        if (file >> freq) return freq;
        return 0;
    }
    
    string getCoreGovernor(int core) {
        string path = "/sys/devices/system/cpu/cpu" + to_string(core) + "/cpufreq/scaling_governor";
        ifstream file(path);
        string gov;
        if (file >> gov) return gov;
        return "unknown";
    }
    
    double getCoreTemperature(int core) {
        vector<string> tempPaths = {
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/thermal/thermal_zone1/temp",
            "/sys/class/thermal/thermal_zone2/temp"
        };
        
        for (const auto& path : tempPaths) {
            ifstream file(path);
            double temp;
            if (file >> temp) {
                return temp / 1000.0;
            }
        }
        return 0.0;
    }
    
public:
    CPUMonitor() {
        coreCount = sysconf(_SC_NPROCESSORS_ONLN);
        prevStats.resize(coreCount + 1);
        prevFreqs.resize(coreCount);
        prevTemps.resize(coreCount);
        update();
    }
    
    void update() {
        ifstream statFile("/proc/stat");
        string line;
        
        getline(statFile, line);
        stringstream ss(line);
        string cpu;
        ss >> cpu >> prevStats[0].user >> prevStats[0].nice >> prevStats[0].system 
           >> prevStats[0].idle >> prevStats[0].iowait >> prevStats[0].irq 
           >> prevStats[0].softirq >> prevStats[0].steal >> prevStats[0].guest 
           >> prevStats[0].guest_nice;
        
        for (int i = 0; i < coreCount; i++) {
            getline(statFile, line);
            stringstream css(line);
            string coreName;
            css >> coreName >> prevStats[i+1].user >> prevStats[i+1].nice >> prevStats[i+1].system 
                >> prevStats[i+1].idle >> prevStats[i+1].iowait >> prevStats[i+1].irq 
                >> prevStats[i+1].softirq;
            prevFreqs[i] = getCoreFrequency(i);
        }
    }
    
    double getTotalUsage() {
        CPUStats prev = prevStats[0];
        usleep(500000);
        
        ifstream statFile("/proc/stat");
        string line;
        getline(statFile, line);
        
        stringstream ss(line);
        string cpu;
        CPUStats curr;
        ss >> cpu >> curr.user >> curr.nice >> curr.system >> curr.idle 
           >> curr.iowait >> curr.irq >> curr.softirq;
        
        long long prevIdle = prev.idle + prev.iowait;
        long long currIdle = curr.idle + curr.iowait;
        
        long long prevTotal = prevIdle + prev.user + prev.nice + prev.system + prev.irq + prev.softirq;
        long long currTotal = currIdle + curr.user + curr.nice + curr.system + curr.irq + curr.softirq;
        
        double totalDiff = currTotal - prevTotal;
        double idleDiff = currIdle - prevIdle;
        
        return (totalDiff > 0) ? (totalDiff - idleDiff) / totalDiff * 100.0 : 0.0;
    }
    
    vector<CPUCore> getAllCores() {
        vector<CPUStats> currStats(coreCount + 1);
        vector<CPUCore> cores;
        
        usleep(500000);
        
        ifstream statFile("/proc/stat");
        string line;
        getline(statFile, line);
        
        for (int i = 0; i < coreCount; i++) {
            getline(statFile, line);
            stringstream css(line);
            string coreName;
            CPUStats curr;
            css >> coreName >> curr.user >> curr.nice >> curr.system >> curr.idle 
                >> curr.iowait >> curr.irq >> curr.softirq;
            
            CPUStats prev = prevStats[i+1];
            
            long long prevIdle = prev.idle + prev.iowait;
            long long currIdle = curr.idle + curr.iowait;
            
            long long prevTotal = prevIdle + prev.user + prev.nice + prev.system + prev.irq + prev.softirq;
            long long currTotal = currIdle + curr.user + curr.nice + curr.system + curr.irq + curr.softirq;
            
            double totalDiff = currTotal - prevTotal;
            double idleDiff = currIdle - prevIdle;
            
            CPUCore core;
            core.id = i;
            core.usage = (totalDiff > 0) ? (totalDiff - idleDiff) / totalDiff * 100.0 : 0.0;
            core.frequency = getCoreFrequency(i);
            core.governor = getCoreGovernor(i);
            core.temperature = getCoreTemperature(i);
            
            cores.push_back(core);
            prevStats[i+1] = curr;
        }
        
        return cores;
    }
    
    int getCoreCount() const { return coreCount; }
    double getTemperature() { return getCoreTemperature(0); }
};

class MemoryMonitor {
public:
    MemoryInfo getInfo() {
        MemoryInfo info = {0};
        
        ifstream memFile("/proc/meminfo");
        string label;
        
        while (memFile >> label) {
            if (label == "MemTotal:") memFile >> info.total;
            else if (label == "MemAvailable:") memFile >> info.available;
            else if (label == "MemFree:") memFile >> info.used;
            else if (label == "Buffers:") memFile >> info.buffers;
            else if (label == "Cached:") memFile >> info.cached;
            else if (label == "SwapTotal:") memFile >> info.swapTotal;
            else if (label == "SwapFree:") memFile >> info.swapFree;
        }
        
        info.total /= 1024;
        info.available /= 1024;
        info.buffers /= 1024;
        info.cached /= 1024;
        info.swapTotal /= 1024;
        info.swapFree /= 1024;
        
        info.used = info.total - info.available;
        info.usagePercent = (info.total > 0) ? (double)info.used / info.total * 100.0 : 0.0;
        
        return info;
    }
};

class BatteryMonitor {
public:
    BatteryInfo getInfo() {
        BatteryInfo info = {0};
        
        ifstream currentFile("/sys/class/power_supply/battery/current_now");
        if (currentFile >> info.current) info.current /= 1000;
        
        ifstream capacityFile("/sys/class/power_supply/battery/capacity");
        if (capacityFile >> info.capacity);
        
        ifstream voltageFile("/sys/class/power_supply/battery/voltage_now");
        if (voltageFile >> info.voltage) info.voltage /= 1000;
        
        ifstream statusFile("/sys/class/power_supply/battery/status");
        if (statusFile >> info.status) {
            info.isCharging = (info.status == "Charging" || info.status == "Full");
        }
        
        ifstream healthFile("/sys/class/power_supply/battery/health");
        if (healthFile >> info.health);
        
        ifstream tempFile("/sys/class/power_supply/battery/temp");
        if (tempFile >> info.temperature) info.temperature /= 10.0;
        
        return info;
    }
};

class NetworkMonitor {
private:
    long long prevRx, prevTx;
    steady_clock::time_point prevTime;
    
public:
    NetworkMonitor() : prevRx(0), prevTx(0) {
        prevTime = steady_clock::now();
    }
    
    NetworkInfo getInfo() {
        NetworkInfo info = {0};
        
        ifstream netFile("/proc/net/dev");
        string line;
        getline(netFile, line);
        getline(netFile, line);
        
        long long totalRx = 0, totalTx = 0;
        
        while (getline(netFile, line)) {
            size_t colonPos = line.find(':');
            if (colonPos == string::npos) continue;
            
            stringstream ss(line.substr(colonPos + 1));
            long long rx, dummy, tx;
            ss >> rx;
            for (int i = 0; i < 7; i++) ss >> dummy;
            ss >> tx;
            
            string interface = line.substr(0, colonPos);
            interface.erase(0, interface.find_first_not_of(" \t"));
            
            if (interface == "lo") continue;
            
            totalRx += rx;
            totalTx += tx;
        }
        
        info.rxBytes = totalRx;
        info.txBytes = totalTx;
        
        auto currentTime = steady_clock::now();
        auto duration = duration_cast<milliseconds>(currentTime - prevTime).count();
        
        if (duration > 0) {
            info.rxSpeed = (totalRx - prevRx) * 1000 / duration;
            info.txSpeed = (totalTx - prevTx) * 1000 / duration;
        }
        
        prevRx = totalRx;
        prevTx = totalTx;
        prevTime = currentTime;
        
        return info;
    }
    
    string formatSpeed(long long bytesPerSec) {
        const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
        int unitIndex = 0;
        double speed = bytesPerSec;
        
        while (speed >= 1024 && unitIndex < 3) {
            speed /= 1024;
            unitIndex++;
        }
        
        stringstream ss;
        ss << fixed << setprecision(1) << setw(6) << speed << " " << units[unitIndex];
        return ss.str();
    }
};

class ProcessMonitor {
public:
    vector<ProcessInfo> getTopProcesses(int count = 10) {
        vector<ProcessInfo> processes;
        
        DIR* dir = opendir("/proc");
        if (!dir) return processes;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;
            
            string pidStr = entry->d_name;
            if (pidStr.find_first_not_of("0123456789") != string::npos) continue;
            
            ProcessInfo info;
            info.pid = stoi(pidStr);
            
            string commPath = "/proc/" + pidStr + "/comm";
            ifstream commFile(commPath);
            if (commFile >> info.name) {
                info.name = info.name.substr(0, 15);
            }
            
            string statPath = "/proc/" + pidStr + "/stat";
            ifstream statFile(statPath);
            if (statFile) {
                string dummy;
                long long utime, stime;
                statFile >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> utime >> stime;
                info.cpuPercent = 0;
            }
            
            string statmPath = "/proc/" + pidStr + "/statm";
            ifstream statmFile(statmPath);
            if (statmFile >> info.memory) {
                info.memory = info.memory * 4 / 1024;
            }
            
            processes.push_back(info);
        }
        
        closedir(dir);
        
        sort(processes.begin(), processes.end(), 
             [](const ProcessInfo& a, const ProcessInfo& b) {
                 return a.memory > b.memory;
             });
        
        if (processes.size() > (size_t)count) {
            processes.resize(count);
        }
        
        return processes;
    }
};

class SystemInfo {
public:
    string getUptime() {
        ifstream uptimeFile("/proc/uptime");
        double uptime;
        if (uptimeFile >> uptime) {
            int days = uptime / 86400;
            int hours = ((int)uptime % 86400) / 3600;
            int minutes = ((int)uptime % 3600) / 60;
            
            stringstream ss;
            if (days > 0) ss << days << "d ";
            ss << hours << "h " << minutes << "m";
            return ss.str();
        }
        return "unknown";
    }
    
    string getLoadAverage() {
        ifstream loadFile("/proc/loadavg");
        double load1, load5, load15;
        if (loadFile >> load1 >> load5 >> load15) {
            stringstream ss;
            ss << fixed << setprecision(2) << load1 << " " << load5 << " " << load15;
            return ss.str();
        }
        return "unknown";
    }
    
    string getKernelVersion() {
        ifstream versionFile("/proc/version");
        string version;
        getline(versionFile, version);
        size_t pos = version.find(" (");
        if (pos != string::npos) {
            version = version.substr(0, pos);
        }
        return version;
    }
    
    string getHostname() {
        ifstream hostnameFile("/proc/sys/kernel/hostname");
        string hostname;
        if (hostnameFile >> hostname) return hostname;
        return "unknown";
    }
};

class DashboardRenderer {
private:
    CPUMonitor cpuMonitor;
    MemoryMonitor memoryMonitor;
    BatteryMonitor batteryMonitor;
    NetworkMonitor networkMonitor;
    ProcessMonitor processMonitor;
    SystemInfo systemInfo;
    
    void drawHeader() {
        auto [cols, rows] = TerminalUtils::getTerminalSize();
        
        string hostname = systemInfo.getHostname();
        string kernel = systemInfo.getKernelVersion();
        string uptime = systemInfo.getUptime();
        
        cout << BG_BLUE << BOLD_WHITE;
        cout << string(cols, ' ') << RESET << "\n";
        
        cout << BG_BLUE << BOLD_WHITE;
        cout << "  ⚡ MIRMEL SYSTEM CONTROL";
        cout << string(cols - 45 - hostname.length(), ' ') << "🏷️ " << hostname;
        cout << RESET << "\n";
        
        cout << DIM;
        cout << "  Kernel: " << kernel.substr(0, min((size_t)50, kernel.length())) << "\n";
        cout << "  Uptime: " << uptime << "  |  Load: " << systemInfo.getLoadAverage();
        cout << RESET << "\n";
        
        cout << string(cols, '─') << "\n";
    }
    
    void drawCPUSection() {
        double totalUsage = cpuMonitor.getTotalUsage();
        double temperature = cpuMonitor.getTemperature();
        auto cores = cpuMonitor.getAllCores();
        
        cout << BOLD_CYAN << "  🖥️  CPU İŞLEMCİ DURUMU" << RESET << "\n";
        cout << "  Toplam Kullanım: " << ProgressBar::generateGradient(totalUsage, 30) << "\n";
        
        cout << "  Sıcaklık: " << TerminalUtils::colorizeTemp(temperature) 
             << fixed << setprecision(1) << temperature << "°C" << RESET << "\n\n";
        
        if (cores.size() <= 8) {
            cout << "  Çekirdek Detayları:\n";
            for (size_t i = 0; i < min((size_t)8, cores.size()); i++) {
                cout << "    CPU" << setw(2) << cores[i].id << ": ";
                cout << ProgressBar::generate(cores[i].usage, 15) << " ";
                cout << setw(4) << (int)(cores[i].frequency / 1000) << "MHz ";
                cout << DIM << cores[i].governor << RESET << "\n";
            }
        }
        
        cout << "\n";
    }
    
    void drawMemorySection() {
        auto mem = memoryMonitor.getInfo();
        
        cout << BOLD_MAGENTA << "  🧠 BELLEK KULLANIMI" << RESET << "\n";
        cout << "  RAM:  " << ProgressBar::generateGradient(mem.usagePercent, 30) << "\n";
        cout << "  " << setw(10) << mem.used << " MB / " << setw(10) << mem.total << " MB\n";
        
        if (mem.swapTotal > 0) {
            double swapUsage = (mem.swapTotal - mem.swapFree) * 100.0 / mem.swapTotal;
            cout << "  SWAP: " << ProgressBar::generate(swapUsage, 30) << "\n";
            cout << "  " << setw(10) << (mem.swapTotal - mem.swapFree) << " MB / " << setw(10) << mem.swapTotal << " MB\n";
        }
        
        cout << "\n";
    }
    
    void drawBatterySection() {
        auto bat = batteryMonitor.getInfo();
        
        cout << BOLD_YELLOW << "  🔋 BATARYA DURUMU" << RESET << "\n";
        
        string statusIcon = bat.isCharging ? "⚡" : "🔋";
        string statusText = bat.isCharging ? "ŞARJ OLUYOR" : "DEŞARJ OLUYOR";
        string statusColor = bat.isCharging ? GREEN : YELLOW;
        
        cout << "  " << statusIcon << " " << statusColor << statusText << RESET << "\n";
        cout << "  Kapasite:  " << ProgressBar::generate((double)bat.capacity, 30) << "\n";
        cout << "  Akım:      " << (bat.current >= 0 ? GREEN : RED) << setw(6) << bat.current << " mA" << RESET;
        cout << "  |  Voltaj: " << bat.voltage << " mV\n";
        
        if (bat.temperature > 0) {
            cout << "  Batarya Sıcaklığı: " << TerminalUtils::colorizeTemp(bat.temperature) 
                 << fixed << setprecision(1) << bat.temperature << "°C" << RESET << "\n";
        }
        
        cout << "\n";
    }
    
    void drawNetworkSection() {
        auto net = networkMonitor.getInfo();
        
        cout << BOLD_GREEN << "  🌐 AĞ TRAFİĞİ" << RESET << "\n";
        cout << "  ↓ İndirme: " << GREEN << networkMonitor.formatSpeed(net.rxSpeed) << RESET;
        cout << "  |  ↑ Yükleme: " << RED << networkMonitor.formatSpeed(net.txSpeed) << RESET << "\n";
        cout << "\n";
    }
    
    void drawProcessSection() {
        auto processes = processMonitor.getTopProcesses(8);
        
        cout << BOLD_BLUE << "  📊 EN ÇOK BELLEK KULLANAN İŞLEMLER" << RESET << "\n";
        cout << "  " << DIM << setw(6) << "PID" << "  " << setw(15) << left << "İSİM" 
             << setw(10) << right << "BELLEK" << RESET << "\n";
        
        for (const auto& proc : processes) {
            if (proc.name.empty()) continue;
            
            cout << "  " << setw(6) << proc.pid << "  ";
            cout << setw(15) << left << proc.name.substr(0, 15);
            
            string memStr;
            if (proc.memory > 1024) {
                stringstream ss;
                ss << fixed << setprecision(1) << (proc.memory / 1024.0) << " GB";
                memStr = ss.str();
            } else {
                memStr = to_string(proc.memory) + " MB";
            }
            
            cout << setw(10) << right << memStr << RESET << "\n";
        }
        cout << "\n";
    }
    
    void drawFooter() {
        auto [cols, rows] = TerminalUtils::getTerminalSize();
        cout << string(cols, '─') << "\n";
        cout << DIM << "  CTRL+C: Çıkış  |  Yenileme: 0.5s" << RESET << "\n";
    }
    
public:
    void render() {
        TerminalUtils::clearScreen();
        drawHeader();
        drawCPUSection();
        drawMemorySection();
        drawBatterySection();
        drawNetworkSection();
        drawProcessSection();
        drawFooter();
    }
};

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    TerminalUtils::hideCursor();
    
    DashboardRenderer dashboard;
    
    try {
        while (g_running) {
            dashboard.render();
            this_thread::sleep_for(milliseconds(500));
        }
    } catch (const exception& e) {
        TerminalUtils::showCursor();
        TerminalUtils::clearScreen();
        cerr << BOLD_RED << "Hata: " << e.what() << RESET << endl;
        return 1;
    }
    
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem monitörü kapatıldı." << RESET << endl;
    
    return 0;
}
