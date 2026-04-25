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

using namespace std;

// --- GELİŞMİŞ RENK MAKROLARI ---
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
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"

// ASCII karakter sabitleri (Unicode yerine)
const string BAR_FULL = "#";
const string BAR_EMPTY = ".";
const string BAR_HALF = ":";
const string BOX_TL = "+";
const string BOX_TR = "+";
const string BOX_BL = "+";
const string BOX_BR = "+";
const string BOX_H = "-";
const string BOX_V = "|";
const string BOX_H_DOUBLE = "=";
const string ARROW_UP = "^";
const string ARROW_DOWN = "v";
const string LIGHTNING = "!";
const string BATTERY = "B";

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
            cols = 80;  // Varsayılan
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
                if (percentage >= 85) bar += RED + BAR_FULL + RESET;
                else if (percentage >= 60) bar += YELLOW + BAR_FULL + RESET;
                else bar += GREEN + BAR_FULL + RESET;
            } else if (i == pos) {
                bar += BOLD + BAR_HALF + RESET;
            } else {
                bar += DIM + BAR_EMPTY + RESET;
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
        for (size_t i = history.size() - 1; i >= history.size() - 5 && i < history.size(); --i) {
            recent += history[i];
            count++;
        }
        
        return (recent / count) - getAverage();
    }
    
    string getGraph(int width = 40, int height = 5) const {
        if (history.size() < 2) return "";
        
        double maxVal = history[0];
        double minVal = history[0];
        
        for (size_t i = 0; i < history.size(); ++i) {
            if (history[i] > maxVal) maxVal = history[i];
            if (history[i] < minVal) minVal = history[i];
        }
        
        if (maxVal == minVal) maxVal = minVal + 1.0;
        
        vector<string> lines(height, string(width, ' '));
        
        for (size_t i = 0; i < history.size() && i < (size_t)width; ++i) {
            double val = history[history.size() - 1 - i];
            int y = (int)((val - minVal) / (maxVal - minVal) * (height - 1));
            y = height - 1 - y;
            
            for (int j = 0; j < height; ++j) {
                if (j == y) {
                    lines[j][width - 1 - i] = '#';
                } else if (j > y) {
                    lines[j][width - 1 - i] = ':';
                }
            }
        }
        
        string graph;
        for (int i = 0; i < height; ++i) {
            graph += "  " + lines[i] + "\n";
        }
        
        stringstream ss;
        ss << "  Min: " << fixed << setprecision(1) << minVal 
           << "%  Max: " << maxVal << "%";
        graph += ss.str();
        
        return graph;
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
        long long user, nice, system, idle, iowait, irq, softirq;
        
        if (!(file >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq)) {
            return 0.0;
        }
        
        long long totalIdle = idle + iowait;
        long long totalCpu = user + nice + system + idle + iowait + irq + softirq;
        
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
    
    string getHistoryGraph() const { 
        return history.getGraph(); 
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
            long long user, nice, system, idle, iowait, irq, softirq;
            
            if (!(iss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq)) {
                continue;
            }
            
            if (coreIndex >= prevCoreIdle.size()) {
                prevCoreIdle.push_back(0);
                prevCoreTotal.push_back(0);
            }
            
            long long totalIdle = idle + iowait;
            long long totalCpu = user + nice + system + idle + iowait + irq + softirq;
            
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
            string path = "/sys/class/thermal/thermal_zone" + to_string(i) + "/type";
            ifstream typeFile(path);
            if (!typeFile.is_open()) continue;
            
            string type;
            typeFile >> type;
            
            if (type.find("cpu") != string::npos || 
                type.find("x86") != string::npos ||
                type.find("acpitz") != string::npos) {
                
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
        long swapTotal = mem["SwapTotal"] * 1024;
        long swapFree = mem["SwapFree"] * 1024;
        
        stats.usedRam = memTotal - memAvailable;
        stats.freeRam = memAvailable;
        stats.cachedRam = cached + buffers;
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
        bool isCharging;
        
        BatData() : current(0), capacity(0), voltage(0.0), power(0.0),
                   cycles(0), health("N/A"), status("Bilinmiyor"),
                   technology("N/A"), temperature(0.0), isCharging(false) {}
    };

private:
    HistoryTracker capHistory;
    HistoryTracker powerHistory;
    
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
            file >> value;
        }
        return value;
    }
    
public:
    BatteryMonitor() : capHistory(60), powerHistory(60) {}

    BatData getStats() {
        BatData data;
        
        // Batarya yolunu bul
        string basePath = "/sys/class/power_supply/BAT0/";
        if (access((basePath + "capacity").c_str(), F_OK) != 0) {
            basePath = "/sys/class/power_supply/BAT1/";
            if (access((basePath + "capacity").c_str(), F_OK) != 0) {
                return data; // Batarya bulunamadı
            }
        }
        
        // Temel değerleri oku
        data.capacity = (int)readLongFromFile(basePath + "capacity");
        long currentRaw = readLongFromFile(basePath + "current_now");
        data.current = currentRaw / 1000; // mA cinsinden
        long voltageRaw = readLongFromFile(basePath + "voltage_now");
        data.voltage = voltageRaw / 1000000.0; // V cinsinden
        
        data.status = readStringFromFile(basePath + "status");
        data.health = readStringFromFile(basePath + "health");
        data.cycles = (int)readLongFromFile(basePath + "cycle_count");
        data.technology = readStringFromFile(basePath + "technology");
        
        // Sıcaklık
        long tempRaw = readLongFromFile(basePath + "temp");
        data.temperature = tempRaw / 10.0;
        
        // Güç hesaplama (Watt)
        data.power = abs(data.voltage * (data.current / 1000.0));
        
        // Şarj durumu
        data.isCharging = (data.status == "Charging" || data.status == "Full");
        
        // Geçmiş kaydı
        capHistory.addValue((double)data.capacity);
        powerHistory.addValue(data.power);
        
        return data;
    }
    
    string getCapacityGraph() const { 
        return capHistory.getGraph(); 
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
        
        // /proc taranıyor
        for (int pid = 1; pid < 32768 && processes.size() < 100; ++pid) {
            string procPath = "/proc/" + to_string(pid);
            if (access(procPath.c_str(), F_OK) != 0) continue;
            
            ProcessInfo proc;
            proc.pid = pid;
            
            // İşlem adını al
            ifstream cmdFile(procPath + "/comm");
            if (!cmdFile.is_open()) continue;
            getline(cmdFile, proc.name);
            if (proc.name.empty()) continue;
            
            // Bellek kullanımını al
            ifstream statusFile(procPath + "/status");
            if (!statusFile.is_open()) continue;
            
            string line;
            while (getline(statusFile, line)) {
                if (line.find("VmRSS:") == 0) {
                    istringstream iss(line.substr(6));
                    long value;
                    string unit;
                    if (iss >> value >> unit) {
                        proc.memSize = value * 1024; // KB -> bytes
                        proc.memPercent = (proc.memSize * 100.0) / totalRam;
                        break;
                    }
                }
            }
            
            if (proc.memSize > 0) {
                processes.push_back(proc);
            }
        }
        
        // Bellek kullanımına göre sırala
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
    // Sinyal yakalayıcıları kur
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
    
    // İlk CPU kalibrasyonu
    cpu.getUsage();
    usleep(100000);
    
    while (g_running) {
        iteration++;
        auto currentTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            currentTime - startTime).count();
        
        // Sistem bilgilerini topla
        double cpuPerc = cpu.getUsage();
        double cpuAvg = cpu.getAverageUsage();
        double temp = cpu.getTemp();
        vector<double> cores = cpu.getCoreUsage();
        
        BatteryMonitor::BatData batData = bat.getStats();
        MemoryMonitor::MemStats memStats = mem.getStats();
        
        TerminalUtils::getTerminalSize(cols, rows);
        
        TerminalUtils::clearScreen();
        
        // Başlık
        cout << BG_BLUE << BOLD_WHITE << "  SYS-MONITOR PRO v4.1 | CALISMA: " 
             << elapsed / 3600 << "s " 
             << (elapsed % 3600) / 60 << "d " 
             << elapsed % 60 << "sn ";
        
        // Sağa dayalı boşluk
        int headerLen = 45;
        int remainingSpace = cols - headerLen;
        if (remainingSpace > 0) {
            cout << string(remainingSpace, ' ');
        }
        cout << RESET << endl;
        
        cout << string(cols, BOX_H[0]) << endl;
        
        // CPU Bölümü
        cout << BOLD_CYAN << BOX_TL << string(2, BOX_H_DOUBLE[0]) 
             << " SISTEM PERFORMANSI " << string(2, BOX_H_DOUBLE[0]) << BOX_TR 
             << RESET << endl;
        
        cout << BOLD_CYAN << BOX_V << RESET 
             << " CPU Yuku:  " << ProgressBar::generate(cpuPerc) << "   "
             << BOLD_CYAN << BOX_V << RESET << endl;
        
        cout << BOLD_CYAN << BOX_V << RESET 
             << " Ortalama:  " << ProgressBar::generate(cpuAvg) << "   "
             << BOLD_CYAN << BOX_V << RESET << endl;
        
        cout << BOLD_CYAN << BOX_V << RESET 
             << " Sicaklik:  ";
        if (temp > 60) cout << BOLD_RED;
        else if (temp > 45) cout << YELLOW;
        else cout << GREEN;
        cout << fixed << setprecision(1) << temp << " C" << RESET;
        cout << string(15, ' ') << BOLD_CYAN << BOX_V << RESET << endl;
        
        // CPU Trend
        double trend = cpu.getTrend();
        if (abs(trend) > 0.5) {
            cout << BOLD_CYAN << BOX_V << RESET 
                 << " Trend:     ";
            if (trend > 0) {
                cout << RED << ARROW_UP << " ARTAN " << abs(trend) << "%" << RESET;
            } else {
                cout << GREEN << ARROW_DOWN << " AZALAN " << abs(trend) << "%" << RESET;
            }
            cout << string(8, ' ') << BOLD_CYAN << BOX_V << RESET << endl;
        }
        
        // CPU Çekirdekleri
        if (!cores.empty() && cols > 60) {
            cout << BOLD_CYAN << BOX_V << RESET << " Cekirdekler: ";
            for (size_t i = 0; i < cores.size() && i < 8; ++i) {
                cout << "CPU" << i << ":" << fixed << setprecision(0) 
                     << setw(3) << cores[i] << "%";
                if (i < cores.size() - 1) cout << " ";
            }
            int usedSpace = 15 + cores.size() * 10;
            if (cols > usedSpace + 2) {
                cout << string(cols - usedSpace - 2, ' ');
            }
            cout << BOLD_CYAN << BOX_V << RESET << endl;
        }
        
        cout << BOLD_CYAN << BOX_BL << string(cols - 2, BOX_H_DOUBLE[0]) 
             << BOX_BR << RESET << endl;
        
        // Bellek Bölümü
        cout << BOLD_YELLOW << "\n" << BOX_TL << string(2, BOX_H_DOUBLE[0]) 
             << " BELLEK DURUMU " << string(2, BOX_H_DOUBLE[0]) << BOX_TR 
             << RESET << endl;
        
        cout << BOLD_YELLOW << BOX_V << RESET 
             << " RAM:       " << ProgressBar::generate(memStats.ramPercent) << "   "
             << BOLD_YELLOW << BOX_V << RESET << endl;
        
        cout << BOLD_YELLOW << BOX_V << RESET 
             << " Kullanilan: " << fixed << setprecision(1) 
             << memStats.usedRam / (1024.0*1024.0) << " MiB / " 
             << (memStats.usedRam + memStats.freeRam) / (1024.0*1024.0) << " MiB"
             << string(cols > 60 ? cols - 60 : 3, ' ')
             << BOLD_YELLOW << BOX_V << RESET << endl;
        
        cout << BOLD_YELLOW << BOX_V << RESET 
             << " Onbellek:  " << memStats.cachedRam / (1024.0*1024.0) << " MiB"
             << string(cols > 40 ? cols - 40 : 5, ' ')
             << BOLD_YELLOW << BOX_V << RESET << endl;
        
        if (memStats.swapPercent > 0) {
            cout << BOLD_YELLOW << BOX_V << RESET 
                 << " Swap:      " << ProgressBar::generate(memStats.swapPercent) << "   "
                 << BOLD_YELLOW << BOX_V << RESET << endl;
        }
        
        cout << BOLD_YELLOW << BOX_BL << string(cols - 2, BOX_H_DOUBLE[0]) 
             << BOX_BR << RESET << endl;
        
        // Güç Bölümü
        cout << BOLD_GREEN << "\n" << BOX_TL << string(2, BOX_H_DOUBLE[0]) 
             << " GUC ANALIZI " << string(2, BOX_H_DOUBLE[0]) << BOX_TR 
             << RESET << endl;
        
        cout << BOLD_GREEN << BOX_V << RESET 
             << " Kapasite:  " << ProgressBar::generate(batData.capacity) << "   "
             << BOLD_GREEN << BOX_V << RESET << endl;
        
        cout << BOLD_GREEN << BOX_V << RESET 
             << " Akim:      ";
        if (batData.current >= 0) cout << GREEN << "+";
        else cout << RED;
        cout << batData.current << " mA" << RESET 
             << " | Guc: " << BOLD << fixed << setprecision(2) 
             << batData.power << " W" << RESET << "   "
             << BOLD_GREEN << BOX_V << RESET << endl;
        
        cout << BOLD_GREEN << BOX_V << RESET 
             << " Voltaj:    " << CYAN << fixed << setprecision(3) 
             << batData.voltage << " V" << RESET;
        if (batData.temperature > 0) {
            cout << " | Sicaklik: " << (int)batData.temperature << "C";
        }
        cout << string(cols > 50 ? cols - 50 : 5, ' ')
             << BOLD_GREEN << BOX_V << RESET << endl;
        
        cout << BOLD_GREEN << BOX_BL << string(cols - 2, BOX_H_DOUBLE[0]) 
             << BOX_BR << RESET << endl;
        
        // Pil Sağlığı Bölümü
        cout << BOLD_MAGENTA << "\n" << BOX_TL << string(2, BOX_H_DOUBLE[0]) 
             << " PIL SAGLIGI " << string(2, BOX_H_DOUBLE[0]) << BOX_TR 
             << RESET << endl;
        
        cout << BOLD_MAGENTA << BOX_V << RESET 
             << " Saglik:    ";
        if (batData.health == "Good") cout << GREEN;
        else cout << YELLOW;
        cout << batData.health << RESET;
        cout << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << BOX_V << RESET << endl;
        
        cout << BOLD_MAGENTA << BOX_V << RESET 
             << " Dongu:     " << WHITE << batData.cycles << " tam dongu" << RESET
             << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << BOX_V << RESET << endl;
        
        cout << BOLD_MAGENTA << BOX_V << RESET 
             << " Teknoloji: " << WHITE << batData.technology << RESET
             << string(cols > 30 ? cols - 30 : 3, ' ')
             << BOLD_MAGENTA << BOX_V << RESET << endl;
        
        cout << BOLD_MAGENTA << BOX_V << RESET 
             << " Durum:     ";
        if (batData.isCharging) {
            double eff = 100.0 - (cpuPerc * 0.4) - (temp > 38 ? (temp - 38) * 2 : 0);
            if (eff < 0) eff = 0;
            cout << GREEN << LIGHTNING << " SARJ OLUYOR" << RESET 
                 << " | Verim: %" << (int)eff;
        } else {
            cout << RED << BATTERY << " DESARJ" << RESET;
        }
        cout << string(cols > 50 ? cols - 50 : 3, ' ')
             << BOLD_MAGENTA << BOX_V << RESET << endl;
        
        cout << BOLD_MAGENTA << BOX_BL << string(cols - 2, BOX_H_DOUBLE[0]) 
             << BOX_BR << RESET << endl;
        
        // En çok bellek kullanan işlemler
        if (cols > 60 && iteration % 3 == 0) {
            cout << BOLD_CYAN << "\n" << BOX_TL << string(2, BOX_H_DOUBLE[0]) 
                 << " EN COK BELLEK KULLANAN ISLEMLER " 
                 << string(2, BOX_H_DOUBLE[0]) << BOX_TR << RESET << endl;
            
            vector<ProcessMonitor::ProcessInfo> processes = proc.getTopProcesses(3);
            for (size_t i = 0; i < processes.size(); ++i) {
                cout << BOLD_CYAN << BOX_V << RESET 
                     << " " << (i + 1) << ". " 
                     << setw(15) << left << processes[i].name.substr(0, 15) 
                     << " PID:" << setw(6) << processes[i].pid 
                     << " Bellek:" << fixed << setprecision(1) << setw(7) 
                     << processes[i].memSize / 1024.0 << " MiB"
                     << string(cols > 60 ? cols - 60 : 2, ' ')
                     << BOLD_CYAN << BOX_V << RESET << endl;
            }
            
            cout << BOLD_CYAN << BOX_BL << string(cols - 2, BOX_H_DOUBLE[0]) 
                 << BOX_BR << RESET << endl;
        }
        
        // Alt bilgi
        cout << endl << string(cols, BOX_H[0]) << endl;
        cout << DIM << " Cikis: CTRL+C | Yenileme: " << iteration 
             << " | Son guncelleme: " << elapsed << "sn" << RESET;
        cout << string(cols > 50 ? cols - 50 : 2, ' ') << endl;
        
        usleep(850000);
    }
    
    // Temizlik
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem monitoru kapatildi. Iyi gunler!" << RESET << endl;
    
    return 0;
}
