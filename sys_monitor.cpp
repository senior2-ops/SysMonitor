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
#include <sys/sysinfo.h>
#include <chrono>
#include <thread>
#include <map>
#include <deque>
#include <numeric>
#include <cstring>
#include <dirent.h>

using namespace std;

// --- RENK MAKROLARI ---
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
#define BOLD_MAGENTA "\033[1;35m"
#define BOLD_WHITE  "\033[1;37m"
#define BG_BLUE     "\033[44m"

volatile sig_atomic_t g_running = 1;

void signalHandler(int signum) { 
    g_running = 0; 
}

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
    
    static void getTerminalSize(int& cols, int& rows) {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            cols = w.ws_col;
            rows = w.ws_row;
        } else {
            cols = 80;
            rows = 24;
        }
    }
};

class ProgressBar {
public:
    static string generate(double percentage, int width = 20) {
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;
        
        string bar = "[";
        int pos = (int)(width * (percentage / 100.0));
        
        for (int i = 0; i < width; ++i) {
            if (i < pos) {
                if (percentage >= 85) bar += RED "#" RESET;
                else if (percentage >= 60) bar += YELLOW "#" RESET;
                else bar += GREEN "#" RESET;
            } else if (i == pos) {
                bar += BOLD ":" RESET;
            } else {
                bar += DIM "." RESET;
            }
        }
        
        bar += "]";
        stringstream ss;
        ss << " %" << fixed << setprecision(1) << percentage;
        bar += ss.str();
        
        return bar;
    }
};

class HistoryTracker {
private:
    deque<double> history;
    size_t maxSize;
    
public:
    HistoryTracker(size_t size = 60) : maxSize(size) {}
    
    void addValue(double value) {
        history.push_back(value);
        if (history.size() > maxSize) {
            history.pop_front();
        }
    }
    
    double getAverage() const {
        if (history.empty()) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < history.size(); ++i) {
            sum += history[i];
        }
        return sum / history.size();
    }
    
    double getTrend() const {
        if (history.size() < 10) return 0.0;
        
        double recent = 0.0;
        size_t count = 0;
        size_t startIdx = history.size() >= 5 ? history.size() - 5 : 0;
        for (size_t i = startIdx; i < history.size(); ++i) {
            recent += history[i];
            count++;
        }
        
        return (count > 0) ? (recent / count) - getAverage() : 0.0;
    }
};

class CPUMonitor {
private:
    long long prevIdle;
    long long prevTotal;
    vector<long long> prevCoreIdle;
    vector<long long> prevCoreTotal;
    HistoryTracker history;
    
public:
    CPUMonitor() : prevIdle(0), prevTotal(0), history(60) {}
    
    double getUsage() {
        ifstream file("/proc/stat");
        if (!file.is_open()) return 0.0;
        
        string label;
        long long user, nice, system, idle, iowait, irq, softirq, steal;
        
        if (!(file >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
            return 0.0;
        }
        
        long long totalIdle = idle + iowait;
        long long totalCpu = user + nice + system + idle + iowait + irq + softirq + steal;
        
        double diffTotal = (double)(totalCpu - prevTotal);
        double diffIdle = (double)(totalIdle - prevIdle);
        
        prevIdle = totalIdle;
        prevTotal = totalCpu;
        
        double usage = (diffTotal > 0.0) ? 
            (1.0 - diffIdle / diffTotal) * 100.0 : 0.0;
        
        history.addValue(usage);
        return usage;
    }
    
    double getAverageUsage() const { 
        return history.getAverage(); 
    }
    
    double getTrend() const { 
        return history.getTrend(); 
    }
    
    vector<double> getCoreUsage() {
        vector<double> coreUsage;
        ifstream file("/proc/stat");
        if (!file.is_open()) return coreUsage;
        
        string line;
        getline(file, line); // Skip aggregate line
        
        size_t coreIndex = 0;
        while (getline(file, line)) {
            if (line.substr(0, 3) != "cpu") break;
            
            istringstream iss(line);
            string label;
            long long user, nice, system, idle, iowait, irq, softirq, steal;
            
            if (!(iss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
                continue;
            }
            
            if (coreIndex >= prevCoreIdle.size()) {
                prevCoreIdle.push_back(0);
                prevCoreTotal.push_back(0);
            }
            
            long long totalIdle = idle + iowait;
            long long totalCpu = user + nice + system + idle + iowait + irq + softirq + steal;
            
            double diffTotal = (double)(totalCpu - prevCoreTotal[coreIndex]);
            double diffIdle = (double)(totalIdle - prevCoreIdle[coreIndex]);
            
            prevCoreIdle[coreIndex] = totalIdle;
            prevCoreTotal[coreIndex] = totalCpu;
            
            double usage = (diffTotal > 0.0) ? 
                (1.0 - diffIdle / diffTotal) * 100.0 : 0.0;
            
            coreUsage.push_back(usage);
            coreIndex++;
        }
        
        return coreUsage;
    }
    
    double getTemp() {
        double highest = 0.0;
        
        // Thermal zone üzerinden okuma
        for (int i = 0; i < 20; ++i) {
            string typePath = "/sys/class/thermal/thermal_zone" + to_string(i) + "/type";
            ifstream typeFile(typePath);
            if (!typeFile.is_open()) continue;
            
            string type;
            typeFile >> type;
            
            if (type.find("cpu") != string::npos || 
                type.find("x86") != string::npos ||
                type.find("acpitz") != string::npos ||
                type.find("soc") != string::npos) {
                
                string tempPath = "/sys/class/thermal/thermal_zone" + to_string(i) + "/temp";
                ifstream tempFile(tempPath);
                double value;
                if (tempFile >> value) {
                    double temp = (value > 1000.0) ? value / 1000.0 : value;
                    if (temp > highest) highest = temp;
                }
            }
        }
        
        // Fallback: hwmon
        if (highest == 0.0) {
            for (int i = 0; i < 10; ++i) {
                for (int j = 1; j <= 3; ++j) {
                    string path = "/sys/class/hwmon/hwmon" + to_string(i) + 
                                 "/temp" + to_string(j) + "_input";
                    ifstream file(path);
                    double value;
                    if (file >> value) {
                        double temp = value / 1000.0;
                        if (temp > highest && temp < 150.0) highest = temp;
                    }
                }
            }
        }
        
        return highest;
    }
};

class MemoryMonitor {
public:
    struct MemStats {
        double ramPercent;
        double swapPercent;
        long usedRam;
        long freeRam;
        long cachedRam;
        long usedSwap;
        long freeSwap;
    };
    
    MemStats getStats() {
        MemStats stats;
        memset(&stats, 0, sizeof(stats));
        
        ifstream file("/proc/meminfo");
        if (!file.is_open()) return stats;
        
        map<string, long> mem;
        string line;
        
        while (getline(file, line)) {
            istringstream iss(line);
            string label;
            long value;
            string unit;
            
            if (iss >> label >> value >> unit) {
                if (!label.empty() && label.back() == ':') {
                    label.pop_back();
                }
                mem[label] = value;
            }
        }
        
        long memTotal = mem["MemTotal"] * 1024;
        long memFree = mem["MemFree"] * 1024;
        long memAvailable = mem["MemAvailable"] * 1024;
        long buffers = mem["Buffers"] * 1024;
        long cached = mem["Cached"] * 1024;
        long sReclaimable = mem["SReclaimable"] * 1024;
        long swapTotal = mem["SwapTotal"] * 1024;
        long swapFree = mem["SwapFree"] * 1024;
        
        stats.usedRam = memTotal - memAvailable;
        stats.freeRam = memAvailable;
        stats.cachedRam = cached + buffers + sReclaimable;
        stats.ramPercent = (memTotal > 0) ? 
            (stats.usedRam * 100.0 / memTotal) : 0.0;
        
        stats.usedSwap = swapTotal - swapFree;
        stats.freeSwap = swapFree;
        stats.swapPercent = (swapTotal > 0) ? 
            (stats.usedSwap * 100.0 / swapTotal) : 0.0;
        
        return stats;
    }
};

class BatteryMonitor {
public:
    struct BatData {
        long current;
        int capacity;
        double voltage;
        double power;
        int cycles;
        string health;
        string status;
        string technology;
        double temperature;
        double capacityRemaining;
        double timeEstimate;
        bool isCharging;
        
        BatData() : current(0), capacity(0), voltage(0.0), power(0.0),
                   cycles(0), health("N/A"), status("Bilinmiyor"),
                   technology("N/A"), temperature(0.0), 
                   capacityRemaining(0.0), timeEstimate(0.0),
                   isCharging(false) {}
    };

private:
    HistoryTracker capHistory;
    HistoryTracker powerHistory;
    string batteryPath;
    
    long readLongFromFile(const string& path) const {
        ifstream file(path);
        long value = 0;
        if (file.is_open()) {
            file >> value;
        }
        return value;
    }
    
    string readStringFromFile(const string& path) const {
        ifstream file(path);
        string value = "N/A";
        if (file.is_open()) {
            getline(file, value);
        }
        return value;
    }
    
    string findBatteryPath() {
        // /sys/class/power_supply/ dizinini tara
        DIR* dir = opendir("/sys/class/power_supply/");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                string name = entry->d_name;
                if (name == "." || name == "..") continue;
                
                string fullPath = "/sys/class/power_supply/" + name + "/";
                string capFile = fullPath + "capacity";
                
                if (access(capFile.c_str(), F_OK) == 0) {
                    ifstream testFile(capFile);
                    int cap;
                    if (testFile >> cap && cap >= 0 && cap <= 100) {
                        closedir(dir);
                        return fullPath;
                    }
                }
            }
            closedir(dir);
        }
        
        return "";
    }
    
    long findChargeFull(const string& path) const {
        // Farklı isimlendirme standartlarını dene
        vector<string> possibleNames = {
            "charge_full", "charge_full_design", 
            "energy_full", "energy_full_design"
        };
        
        for (const auto& name : possibleNames) {
            long value = readLongFromFile(path + name);
            if (value > 0) return value;
        }
        
        return 0;
    }
    
public:
    BatteryMonitor() : capHistory(60), powerHistory(60) {
        batteryPath = findBatteryPath();
    }

    BatData getStats() {
        BatData data;
        
        if (batteryPath.empty()) {
            batteryPath = findBatteryPath();
            if (batteryPath.empty()) return data;
        }
        
        // Kapasite (%)
        data.capacity = (int)readLongFromFile(batteryPath + "capacity");
        if (data.capacity < 0 || data.capacity > 100) {
            data.capacity = 0;
        }
        
        // Akım (mA)
        long currentRaw = readLongFromFile(batteryPath + "current_now");
        data.current = currentRaw / 1000;
        
        // Voltaj (V)
        long voltageRaw = readLongFromFile(batteryPath + "voltage_now");
        data.voltage = voltageRaw / 1000000.0;
        if (data.voltage <= 0) {
            data.voltage = voltageRaw / 1000.0;  // Bazı sistemlerde mV
        }
        
        // Durum
        data.status = readStringFromFile(batteryPath + "status");
        
        // Sağlık
        data.health = readStringFromFile(batteryPath + "health");
        if (data.health == "N/A" || data.health.empty()) {
            data.health = "Good";
        }
        
        // Döngü sayısı
        data.cycles = (int)readLongFromFile(batteryPath + "cycle_count");
        
        // Teknoloji
        data.technology = readStringFromFile(batteryPath + "technology");
        if (data.technology == "N/A" || data.technology.empty()) {
            data.technology = "Li-ion";
        }
        
        // Sıcaklık
        long tempRaw = readLongFromFile(batteryPath + "temp");
        if (tempRaw > 0) {
            if (tempRaw > 100) {
                data.temperature = tempRaw / 10.0;
            } else {
                data.temperature = (double)tempRaw;
            }
        }
        
        // Güç hesaplama (Watt)
        data.power = abs(data.voltage * (data.current / 1000.0));
        
        // Şarj durumu
        data.isCharging = (data.status == "Charging" || 
                          data.status == "Full" || 
                          data.status == "charging" ||
                          data.status == "full");
        
        // Kalan kapasite (Wh) ve tahmini süre
        long chargeFull = findChargeFull(batteryPath);
        if (chargeFull > 0 && data.voltage > 0) {
            double fullCapacityWh = (chargeFull / 1000000.0) * data.voltage;
            data.capacityRemaining = (data.capacity / 100.0) * fullCapacityWh;
            
            if (data.power > 0.1) {
                if (!data.isCharging) {
                    data.timeEstimate = (data.capacityRemaining / data.power) * 60.0;
                } else {
                    double remaining = fullCapacityWh - data.capacityRemaining;
                    data.timeEstimate = (remaining / data.power) * 60.0;
                }
            }
        }
        
        // Geçmiş kaydı
        capHistory.addValue((double)data.capacity);
        powerHistory.addValue(data.power);
        
        return data;
    }
};

class ProcessMonitor {
public:
    struct ProcessInfo {
        string name;
        int pid;
        long memSize;
        double memPercent;
    };
    
    vector<ProcessInfo> getTopProcesses(int count = 5) {
        vector<ProcessInfo> processes;
        
        struct sysinfo sysInfo;
        if (sysinfo(&sysInfo) != 0) return processes;
        
        long totalRam = sysInfo.totalram * sysInfo.mem_unit;
        if (totalRam <= 0) return processes;
        
        for (int pid = 1; pid < 32768 && processes.size() < 100; ++pid) {
            string procPath = "/proc/" + to_string(pid);
            if (access(procPath.c_str(), F_OK) != 0) continue;
            
            ProcessInfo proc;
            proc.pid = pid;
            
            ifstream cmdFile(procPath + "/comm");
            if (!cmdFile.is_open()) continue;
            getline(cmdFile, proc.name);
            if (proc.name.empty()) continue;
            
            ifstream statusFile(procPath + "/status");
            if (!statusFile.is_open()) continue;
            
            string line;
            while (getline(statusFile, line)) {
                if (line.find("VmRSS:") == 0) {
                    istringstream iss(line.substr(6));
                    long value;
                    string unit;
                    if (iss >> value >> unit) {
                        proc.memSize = value * 1024;
                        proc.memPercent = (proc.memSize * 100.0) / totalRam;
                        break;
                    }
                }
            }
            
            if (proc.memSize > 0) {
                processes.push_back(proc);
            }
        }
        
        sort(processes.begin(), processes.end(),
             [](const ProcessInfo& a, const ProcessInfo& b) {
                 return a.memSize > b.memSize;
             });
        
        if (processes.size() > (size_t)count) {
            processes.resize(count);
        }
        
        return processes;
    }
};

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    TerminalUtils::hideCursor();
    
    CPUMonitor cpu;
    BatteryMonitor bat;
    MemoryMonitor mem;
    ProcessMonitor proc;
    
    auto startTime = chrono::steady_clock::now();
    int iteration = 0;
    int cols = 80, rows = 24;
    
    // CPU kalibrasyonu
    cpu.getUsage();
    usleep(100000);
    
    while (g_running) {
        iteration++;
        auto currentTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            currentTime - startTime).count();
        
        double cpuPerc = cpu.getUsage();
        double cpuAvg = cpu.getAverageUsage();
        double temp = cpu.getTemp();
        vector<double> cores = cpu.getCoreUsage();
        
        BatteryMonitor::BatData batData = bat.getStats();
        MemoryMonitor::MemStats memStats = mem.getStats();
        
        TerminalUtils::getTerminalSize(cols, rows);
        TerminalUtils::clearScreen();
        
        // Başlık
        cout << BG_BLUE << BOLD_WHITE << "  SYS-MONITOR PRO v4.2 | CALISMA: " 
             << elapsed / 3600 << "s " 
             << (elapsed % 3600) / 60 << "d " 
             << elapsed % 60 << "sn ";
        
        int headerLen = 45;
        int remainingSpace = cols - headerLen;
        if (remainingSpace > 0) {
            cout << string(remainingSpace, ' ');
        }
        cout << RESET << endl;
        
        cout << string(cols, '-') << endl;
        
        // CPU Bölümü
        cout << BOLD_CYAN << "+== SISTEM PERFORMANSI ==+" << RESET << endl;
        cout << BOLD_CYAN << "|" << RESET 
             << " CPU Yuku:  " << ProgressBar::generate(cpuPerc) << "   "
             << BOLD_CYAN << "|" << RESET << endl;
        
        cout << BOLD_CYAN << "|" << RESET 
             << " Ortalama:  " << ProgressBar::generate(cpuAvg) << "   "
             << BOLD_CYAN << "|" << RESET << endl;
        
        cout << BOLD_CYAN << "|" << RESET 
             << " Sicaklik:  ";
        if (temp > 60) cout << BOLD_RED;
        else if (temp > 45) cout << YELLOW;
        else cout << GREEN;
        cout << fixed << setprecision(1) << temp << " C" << RESET;
        cout << string(15, ' ') << BOLD_CYAN << "|" << RESET << endl;
        
        double trend = cpu.getTrend();
        if (abs(trend) > 0.5) {
            cout << BOLD_CYAN << "|" << RESET 
                 << " Trend:     ";
            if (trend > 0) {
                cout << RED << "^ ARTAN " << abs(trend) << "%" << RESET;
            } else {
                cout << GREEN << "v AZALAN " << abs(trend) << "%" << RESET;
            }
            cout << string(8, ' ') << BOLD_CYAN << "|" << RESET << endl;
        }
        
        if (!cores.empty() && cols > 60) {
            cout << BOLD_CYAN << "|" << RESET << " Cekirdekler: ";
            for (size_t i = 0; i < cores.size() && i < 8; ++i) {
                cout << "CPU" << i << ":" << fixed << setprecision(0) 
                     << setw(3) << cores[i] << "%";
                if (i < cores.size() - 1) cout << " ";
            }
            int usedSpace = 15 + min(cores.size(), (size_t)8) * 10;
            if (cols > usedSpace + 2) {
                cout << string(cols - usedSpace - 2, ' ');
            }
            cout << BOLD_CYAN << "|" << RESET << endl;
        }
        
        cout << BOLD_CYAN << "+" << string(cols - 2, '=') << "+" << RESET << endl;
        
        // Bellek Bölümü
        cout << BOLD_YELLOW << "\n+== BELLEK DURUMU ==+" << RESET << endl;
        
        cout << BOLD_YELLOW << "|" << RESET 
             << " RAM:       " << ProgressBar::generate(memStats.ramPercent) << "   "
             << BOLD_YELLOW << "|" << RESET << endl;
        
        cout << BOLD_YELLOW << "|" << RESET 
             << " Kullanilan: " << fixed << setprecision(1) 
             << memStats.usedRam / (1024.0*1024.0) << " MiB / " 
             << (memStats.usedRam + memStats.freeRam) / (1024.0*1024.0) << " MiB"
             << string(cols > 60 ? cols - 60 : 3, ' ')
             << BOLD_YELLOW << "|" << RESET << endl;
        
        cout << BOLD_YELLOW << "|" << RESET 
             << " Onbellek:  " << memStats.cachedRam / (1024.0*1024.0) << " MiB"
             << string(cols > 40 ? cols - 40 : 5, ' ')
             << BOLD_YELLOW << "|" << RESET << endl;
        
        if (memStats.swapPercent > 0) {
            cout << BOLD_YELLOW << "|" << RESET 
                 << " Swap:      " << ProgressBar::generate(memStats.swapPercent) << "   "
                 << BOLD_YELLOW << "|" << RESET << endl;
        }
        
        cout << BOLD_YELLOW << "+" << string(cols - 2, '=') << "+" << RESET << endl;
        
        // Güç Bölümü
        cout << BOLD_GREEN << "\n+== GUC ANALIZI ==+" << RESET << endl;
        
        if (batData.capacity > 0) {
            cout << BOLD_GREEN << "|" << RESET 
                 << " Kapasite:  " << ProgressBar::generate(batData.capacity) << "   "
                 << BOLD_GREEN << "|" << RESET << endl;
        } else {
            cout << BOLD_GREEN << "|" << RESET 
                 << " Kapasite:  Pil bulunamadi veya veri alinamiyor"
                 << string(cols > 55 ? cols - 55 : 3, ' ')
                 << BOLD_GREEN << "|" << RESET << endl;
        }
        
        cout << BOLD_GREEN << "|" << RESET 
             << " Akim:      ";
        if (batData.current >= 0) cout << GREEN << "+";
        else cout << RED;
        cout << batData.current << " mA" << RESET 
             << " | Guc: " << BOLD << fixed << setprecision(2) 
             << batData.power << " W" << RESET << "   "
             << BOLD_GREEN << "|" << RESET << endl;
        
        cout << BOLD_GREEN << "|" << RESET 
             << " Voltaj:    " << CYAN << fixed << setprecision(3) 
             << batData.voltage << " V" << RESET;
        if (batData.temperature > 0) {
            cout << " | Sicaklik: " << (int)batData.temperature << "C";
        }
        cout << string(cols > 50 ? cols - 50 : 5, ' ')
             << BOLD_GREEN << "|" << RESET << endl;
        
        if (batData.capacityRemaining > 0) {
            cout << BOLD_GREEN << "|" << RESET 
                 << " Kalan:     " << fixed << setprecision(2) 
                 << batData.capacityRemaining << " Wh";
            if (batData.timeEstimate > 0) {
                int hours = (int)(batData.timeEstimate / 60);
                int mins = (int)(batData.timeEstimate) % 60;
                cout << " | Tahmini: " << hours << "s " << mins << "d";
            }
            cout << string(cols > 60 ? cols - 60 : 3, ' ')
                 << BOLD_GREEN << "|" << RESET << endl;
        }
        
        cout << BOLD_GREEN << "+" << string(cols - 2, '=') << "+" << RESET << endl;
        
        // Pil Sağlığı Bölümü
        cout << BOLD_MAGENTA << "\n+== PIL SAGLIGI ==+" << RESET << endl;
        
        cout << BOLD_MAGENTA << "|" << RESET 
             << " Saglik:    ";
        if (batData.health == "Good") cout << GREEN;
        else cout << YELLOW;
        cout << batData.health << RESET;
        cout << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << "|" << RESET << endl;
        
        cout << BOLD_MAGENTA << "|" << RESET 
             << " Dongu:     " << WHITE << batData.cycles << " tam dongu" << RESET
             << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << "|" << RESET << endl;
        
        cout << BOLD_MAGENTA << "|" << RESET 
             << " Teknoloji: " << WHITE << batData.technology << RESET
             << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << "|" << RESET << endl;
        
        cout << BOLD_MAGENTA << "|" << RESET 
             << " Durum:     ";
        if (batData.isCharging) {
            double eff = 100.0 - (cpuPerc * 0.4) - (temp > 38 ? (temp - 38) * 2 : 0);
            if (eff < 0) eff = 0;
            cout << GREEN << "! SARJ OLUYOR" << RESET 
                 << " | Verim: %" << (int)eff;
        } else {
            cout << RED << "B DESARJ" << RESET;
        }
        cout << string(cols > 50 ? cols - 50 : 3, ' ')
             << BOLD_MAGENTA << "|" << RESET << endl;
        
        cout << BOLD_MAGENTA << "+" << string(cols - 2, '=') << "+" << RESET << endl;
        
        // En çok bellek kullanan işlemler
        if (cols > 60 && iteration % 3 == 0) {
            cout << BOLD_CYAN << "\n+== EN COK BELLEK KULLANAN ISLEMLER ==+" << RESET << endl;
            
            vector<ProcessMonitor::ProcessInfo> processes = proc.getTopProcesses(3);
            for (size_t i = 0; i < processes.size(); ++i) {
                cout << BOLD_CYAN << "|" << RESET 
                     << " " << (i + 1) << ". " 
                     << setw(15) << left << processes[i].name.substr(0, 15) 
                     << " PID:" << setw(6) << processes[i].pid 
                     << " Bellek:" << fixed << setprecision(1) << setw(7) 
                     << processes[i].memSize / 1024.0 << " MiB"
                     << string(cols > 60 ? cols - 60 : 2, ' ')
                     << BOLD_CYAN << "|" << RESET << endl;
            }
            
            cout << BOLD_CYAN << "+" << string(cols - 2, '=') << "+" << RESET << endl;
        }
        
        // Alt bilgi
        cout << endl << string(cols, '-') << endl;
        cout << DIM << " Cikis: CTRL+C | Yenileme: " << iteration 
             << " | Son guncelleme: " << elapsed << "sn" << RESET;
        cout << string(cols > 50 ? cols - 50 : 2, ' ') << endl;
        
        usleep(850000);
    }
    
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem monitoru kapatildi. Iyi gunler!" << RESET << endl;
    
    return 0;
}
