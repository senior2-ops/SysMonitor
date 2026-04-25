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

volatile sig_atomic_t g_running = 1;
void signalHandler(int signum) { g_running = 0; }

class TerminalUtils {
public:
    static void clearScreen() { cout << "\033[2J\033[1;1H"; }
    static void hideCursor() { cout << "\033[?25l"; }
    static void showCursor() { cout << "\033[?25h"; }
    static pair<int, int> getTerminalSize() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return {w.ws_col, w.ws_row};
    }
};

class ProgressBar {
public:
    static string generate(double percentage, int width = 20, bool showPercent = true) {
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;
        
        string bar = "[";
        int pos = (int)(width * (percentage / 100.0));
        for (int i = 0; i < width; ++i) {
            if (i < pos) {
                if (percentage >= 85) bar += RED "█" RESET;
                else if (percentage >= 60) bar += YELLOW "█" RESET;
                else bar += GREEN "█" RESET;
            } else if (i == pos && showPercent) {
                bar += BOLD "▏" RESET;
            } else {
                bar += DIM "░" RESET;
            }
        }
        bar += "]";
        if (showPercent) {
            stringstream ss;
            ss << " " << fixed << setprecision(1) << percentage << "%";
            bar += ss.str();
        }
        return bar;
    }
};

class MemoryMonitor {
private:
    long totalRam = 0, totalSwap = 0;
    
public:
    MemoryMonitor() {
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            totalRam = info.totalram * info.mem_unit;
            totalSwap = info.totalswap * info.mem_unit;
        }
    }
    
    struct MemStats {
        double ramPercent, swapPercent;
        long usedRam, freeRam, cachedRam;
        long usedSwap, freeSwap;
    };
    
    MemStats getStats() {
        MemStats m = {0, 0, 0, 0, 0, 0, 0};
        ifstream file("/proc/meminfo");
        string label;
        long value;
        map<string, long> mem;
        
        while (file >> label >> value) {
            mem[label.substr(0, label.length()-1)] = value;
        }
        
        long memTotal = mem["MemTotal"] * 1024;
        long memFree = mem["MemFree"] * 1024;
        long memAvailable = mem["MemAvailable"] * 1024;
        long buffers = mem["Buffers"] * 1024;
        long cached = mem["Cached"] * 1024;
        long swapTotal = mem["SwapTotal"] * 1024;
        long swapFree = mem["SwapFree"] * 1024;
        
        m.usedRam = memTotal - memAvailable;
        m.freeRam = memAvailable;
        m.cachedRam = cached + buffers;
        m.ramPercent = (memTotal > 0) ? (m.usedRam * 100.0 / memTotal) : 0;
        
        m.usedSwap = swapTotal - swapFree;
        m.freeSwap = swapFree;
        m.swapPercent = (swapTotal > 0) ? (m.usedSwap * 100.0 / swapTotal) : 0;
        
        return m;
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
        if (history.size() > maxSize) history.pop_front();
    }
    
    double getAverage() {
        if (history.empty()) return 0;
        return accumulate(history.begin(), history.end(), 0.0) / history.size();
    }
    
    double getTrend() {
        if (history.size() < 10) return 0;
        auto it = history.rbegin();
        double recent = 0;
        for (int i = 0; i < 5 && it != history.rend(); ++i, ++it) recent += *it;
        return (recent / 5) - getAverage();
    }
    
    string getGraph(int width = 40, int height = 5) {
        if (history.size() < 2) return "";
        
        double maxVal = *max_element(history.begin(), history.end());
        double minVal = *min_element(history.begin(), history.end());
        if (maxVal == minVal) maxVal = minVal + 1;
        
        vector<string> lines(height, string(width, ' '));
        
        for (size_t i = 0; i < history.size() && i < width; i++) {
            double val = history[history.size() - 1 - i];
            int y = (int)((val - minVal) / (maxVal - minVal) * (height - 1));
            y = height - 1 - y; // Flip Y axis
            
            for (int j = 0; j < height; j++) {
                if (j == y) lines[j][width - 1 - i] = '█';
                else if (j > y) lines[j][width - 1 - i] = '▄';
                else lines[j][width - 1 - i] = ' ';
            }
        }
        
        string graph;
        for (const auto& line : lines) {
            graph += "  " + line + "\n";
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
    long long prevIdle = 0, prevTotal = 0;
    vector<long long> prevCoreIdle, prevCoreTotal;
    HistoryTracker history;
    
public:
    CPUMonitor() : history(60) {}
    
    double getUsage() {
        ifstream file("/proc/stat");
        string label;
        long long u, n, s, i, io, ir, si;
        if (!(file >> label >> u >> n >> s >> i >> io >> ir >> si)) return 0.0;
        
        long long idle = i + io;
        long long total = u + n + s + i + io + ir + si + 0; // steal time eklenebilir
        
        double diffTotal = total - prevTotal;
        double diffIdle = idle - prevIdle;
        prevIdle = idle; prevTotal = total;
        
        double usage = (diffTotal > 0) ? (1.0 - ((double)diffIdle / diffTotal)) * 100.0 : 0.0;
        history.addValue(usage);
        return usage;
    }
    
    double getAverageUsage() { return history.getAverage(); }
    double getTrend() { return history.getTrend(); }
    string getHistoryGraph() { return history.getGraph(); }
    
    vector<double> getCoreUsage() {
        vector<double> coreUsage;
        ifstream file("/proc/stat");
        string line;
        getline(file, line); // Skip first line
        
        while (getline(file, line)) {
            if (line.substr(0, 3) != "cpu") break;
            
            istringstream iss(line);
            string label;
            long long u, n, s, i, io, ir, si;
            iss >> label >> u >> n >> s >> i >> io >> ir >> si;
            
            if (prevCoreIdle.empty()) {
                prevCoreIdle.resize(coreUsage.size() + 1, 0);
                prevCoreTotal.resize(coreUsage.size() + 1, 0);
            }
            
            size_t coreIdx = coreUsage.size();
            long long idle = i + io;
            long long total = u + n + s + i + io + ir + si;
            
            double diffTotal = total - prevCoreTotal[coreIdx];
            double diffIdle = idle - prevCoreIdle[coreIdx];
            
            prevCoreIdle[coreIdx] = idle;
            prevCoreTotal[coreIdx] = total;
            
            coreUsage.push_back((diffTotal > 0) ? 
                (1.0 - (double)diffIdle / diffTotal) * 100.0 : 0.0);
        }
        
        return coreUsage;
    }
    
    double getTemp() {
        const vector<string> targets = {"x86_pkg_temp", "cpu-thermal", "cpu_thermal", 
                                        "battery", "cpu-0-0-usr", "acpitz"};
        double highest = 0;
        
        for (const auto& t : targets) {
            for (int i = 0; i < 60; i++) {
                string path = "/sys/class/thermal/thermal_zone" + to_string(i);
                ifstream typeFile(path + "/type");
                string type;
                if (typeFile >> type && type == t) {
                    ifstream tempFile(path + "/temp");
                    double v;
                    if (tempFile >> v) {
                        double temp = (v > 1000 ? v / 1000.0 : v);
                        highest = max(highest, temp);
                    }
                }
            }
        }
        
        // Fallback: hwmon üzerinden sıcaklık okuma
        if (highest == 0) {
            for (int i = 0; i < 10; i++) {
                string path = "/sys/class/hwmon/hwmon" + to_string(i) + "/temp1_input";
                ifstream tempFile(path);
                double v;
                if (tempFile >> v) {
                    highest = max(highest, v / 1000.0);
                }
            }
        }
        
        return highest;
    }
};

class BatteryMonitor {
private:
    HistoryTracker capHistory;
    HistoryTracker powerHistory;
    
public:
    BatteryMonitor() : capHistory(60), powerHistory(60) {}
    
    struct BatData {
        long current;
        int capacity = 0;
        double voltage = 0.0;
        double power = 0.0;
        int cycles = 0;
        string health = "N/A";
        string status = "Bilinmiyor";
        string technology = "N/A";
        double temperature = 0.0;
        double capacityRemaining = 0.0;
        double timeEstimate = 0.0;
    };

    BatData getStats() {
        BatData d;
        
        auto readLong = [](const string& path) -> long {
            ifstream f(path);
            long val = 0;
            if (f >> val) return val;
            return 0;
        };
        
        auto readStr = [](const string& path) -> string {
            ifstream f(path);
            string s;
            if (f >> s) return s;
            return "N/A";
        };
        
        string base = "/sys/class/power_supply/BAT0/";
        if (access((base + "capacity").c_str(), F_OK) != 0) {
            base = "/sys/class/power_supply/battery/";
        }
        
        d.capacity = readLong(base + "capacity");
        long currentNow = readLong(base + "current_now");
        d.current = currentNow / 1000; // Convert to mA
        long voltageNow = readLong(base + "voltage_now");
        d.voltage = voltageNow / 1000000.0;
        d.status = readStr(base + "status");
        d.health = readStr(base + "health");
        d.cycles = readLong(base + "cycle_count");
        d.technology = readStr(base + "technology");
        
        // Sıcaklık okuması
        d.temperature = readLong(base + "temp") / 10.0;
        
        // Güç hesaplama
        d.power = abs(d.voltage * (d.current / 1000.0));
        
        // Kapasite kalan (Wh cinsinden)
        ifstream chargeFull(base + "charge_full");
        long chargeFullVal;
        if (chargeFull >> chargeFullVal) {
            d.capacityRemaining = (d.capacity / 100.0) * (chargeFullVal / 1000000.0) * d.voltage;
        }
        
        // Tahmini süre hesaplama
        if (d.power > 0) {
            if (d.current < 0) { // Deşarj
                d.timeEstimate = (d.capacityRemaining / d.power) * 60; // Dakika
            } else if (d.current > 0) { // Şarj
                double remainingCapacity = (100 - d.capacity) / 100.0 * 
                    (chargeFullVal / 1000000.0) * d.voltage;
                d.timeEstimate = (remainingCapacity / d.power) * 60;
            }
        }
        
        capHistory.addValue(d.capacity);
        powerHistory.addValue(d.power);
        
        return d;
    }
    
    string getCapacityGraph() { return capHistory.getGraph(); }
};

class ProcessMonitor {
public:
    struct Process {
        string name;
        int pid;
        double cpuPercent;
        double memPercent;
        long memSize;
    };
    
    vector<Process> getTopProcesses(int count = 5) {
        vector<Process> processes;
        
        // /proc dizini taranarak process bilgileri alınır
        for (int pid = 1; pid < 32768 && processes.size() < 50; pid++) {
            string path = "/proc/" + to_string(pid);
            if (access(path.c_str(), F_OK) != 0) continue;
            
            Process p;
            p.pid = pid;
            
            // Process adı
            ifstream cmdFile(path + "/comm");
            if (cmdFile >> p.name) {
                // Basitleştirilmiş CPU kullanımı
                ifstream statFile(path + "/stat");
                string statLine;
                if (getline(statFile, statLine)) {
                    // Basit bir hesaplama (gerçek uygulamada daha detaylı olmalı)
                    p.cpuPercent = 0.0; // Placeholder
                }
                
                // Memory kullanımı
                ifstream statusFile(path + "/status");
                string line;
                while (getline(statusFile, line)) {
                    if (line.find("VmRSS:") != string::npos) {
                        istringstream iss(line.substr(6));
                        iss >> p.memSize;
                        break;
                    }
                }
                
                if (p.memSize > 0) {
                    struct sysinfo info;
                    sysinfo(&info);
                    p.memPercent = (p.memSize * 1024.0) / (info.totalram * info.mem_unit) * 100;
                    processes.push_back(p);
                }
            }
        }
        
        // Memory kullanımına göre sırala
        sort(processes.begin(), processes.end(), 
             [](const Process& a, const Process& b) { return a.memSize > b.memSize; });
        
        if (processes.size() > count) processes.resize(count);
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
    
    // İlk okuma (CPU kalibrasyonu için)
    cpu.getUsage();
    usleep(100000);
    
    while (g_running) {
        iteration++;
        auto currentTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(currentTime - startTime).count();
        
        double cpuPerc = cpu.getUsage();
        double cpuAvg = cpu.getAverageUsage();
        double temp = cpu.getTemp();
        auto cores = cpu.getCoreUsage();
        auto b = bat.getStats();
        auto memStats = mem.getStats();
        auto [cols, rows] = TerminalUtils::getTerminalSize();

        TerminalUtils::clearScreen();
        
        // Başlık
        cout << BG_BLUE << BOLD_WHITE << "  SYS-MONITOR PRO v4.0 | ÇALIŞMA: " 
             << elapsed / 3600 << "s " << (elapsed % 3600) / 60 << "d " 
             << elapsed % 60 << "sn | " << string(cols > 60 ? cols - 60 : 0, ' ') 
             << RESET << endl;
        
        cout << string(cols, '─') << endl;

        // CPU Bölümü
        cout << BOLD_CYAN << "╔══ SİSTEM PERFORMANSI ══╗" << RESET << endl;
        cout << "║ CPU Yükü:  " << ProgressBar::generate(cpuPerc) << "   ║" << endl;
        cout << "║ Ortalama:  " << ProgressBar::generate(cpuAvg) << "   ║" << endl;
        cout << "║ Sıcaklık:  " << (temp > 60 ? BOLD_RED : temp > 45 ? YELLOW : GREEN) 
             << fixed << setprecision(1) << temp << " °C" << RESET 
             << string(15, ' ') << "║" << endl;
        
        // CPU Trend
        double trend = cpu.getTrend();
        if (abs(trend) > 0.5) {
            cout << "║ Trend:     " << (trend > 0 ? RED "↑ ARTAN" : GREEN "↓ AZALAN") 
                 << " " << abs(trend) << "%" << RESET << string(10, ' ') << "║" << endl;
        }
        
        // CPU Çekirdekleri
        if (!cores.empty() && cols > 60) {
            cout << "║ Çekirdekler: ";
            int coresPerRow = min((int)cores.size(), 4);
            for (size_t i = 0; i < cores.size(); i++) {
                cout << "CPU" << i << ":" << fixed << setprecision(0) << setw(3) << cores[i] << "%";
                if ((i + 1) % coresPerRow == 0 && i != cores.size() - 1) {
                    cout << endl << "║              ";
                } else if (i != cores.size() - 1) {
                    cout << " ";
                }
            }
            cout << string(cols > 80 ? cols - 80 : 5, ' ') << "║" << endl;
        }
        
        cout << "╚" << string(cols - 3, '═') << "╝" << endl;

        // Bellek Bölümü
        cout << BOLD_YELLOW << "\n╔══ BELLEK DURUMU ══╗" << RESET << endl;
        cout << "║ RAM:       " << ProgressBar::generate(memStats.ramPercent) << "   ║" << endl;
        cout << "║ Kullanılan:" << fixed << setprecision(1) 
             << memStats.usedRam / (1024.0*1024.0) << " MiB / " 
             << (memStats.usedRam + memStats.freeRam) / (1024.0*1024.0) << " MiB"
             << string(cols > 60 ? cols - 60 : 3, ' ') << "║" << endl;
        cout << "║ Önbellek:  " << memStats.cachedRam / (1024.0*1024.0) << " MiB" 
             << string(cols > 40 ? cols - 40 : 5, ' ') << "║" << endl;
        
        if (memStats.swapPercent > 0) {
            cout << "║ Swap:      " << ProgressBar::generate(memStats.swapPercent) << "   ║" << endl;
        }
        cout << "╚" << string(cols - 3, '═') << "╝" << endl;

        // Güç Bölümü
        cout << BOLD_GREEN << "\n╔══ GÜÇ ANALİZİ ══╗" << RESET << endl;
        cout << "║ Kapasite:  " << ProgressBar::generate(b.capacity) << "   ║" << endl;
        cout << "║ Akım:      " << (b.current >= 0 ? GREEN "+" : RED) 
             << b.current << " mA" << RESET << " | Güç: " << BOLD 
             << fixed << setprecision(2) << b.power << " W" << RESET << "   ║" << endl;
        cout << "║ Voltaj:    " << CYAN << fixed << setprecision(3) << b.voltage << " V" << RESET 
             << (b.temperature > 0 ? " | Sıcaklık: " + to_string((int)b.temperature) + "°C" : "") 
             << "   ║" << endl;
        
        if (b.power > 0 && b.timeEstimate > 0) {
            int hours = (int)(b.timeEstimate / 60);
            int mins = (int)(b.timeEstimate) % 60;
            cout << "║ Tahmini:   " << MAGENTA << hours << "s " << mins << "d" 
                 << (b.current > 0 ? " (tam şarj)" : " (bitiş)") << RESET << "   ║" << endl;
        }
        
        cout << "╚" << string(cols - 3, '═') << "╝" << endl;

        // Pil Sağlığı Bölümü
        cout << BOLD_MAGENTA << "\n╔══ PİL SAĞLIĞI ══╗" << RESET << endl;
        cout << "║ Sağlık:    " << (b.health == "Good" ? GREEN : YELLOW) 
             << b.health << RESET << string(cols > 30 ? cols - 30 : 3, ' ') << "║" << endl;
        cout << "║ Döngü:     " << WHITE << b.cycles << " tam döngü" << RESET 
             << string(cols > 30 ? cols - 30 : 3, ' ') << "║" << endl;
        cout << "║ Teknoloji: " << WHITE << b.technology << RESET 
             << string(cols > 30 ? cols - 30 : 3, ' ') << "║" << endl;
        
        if (b.charging || b.status == "Charging" || b.status == "Full") {
            double eff = 100.0 - (cpuPerc * 0.4) - (temp > 38 ? (temp - 38) * 2 : 0);
            if (eff < 0) eff = 0;
            cout << "║ Durum:     " << GREEN << "⚡ ŞARJ OLUYOR" << RESET 
                 << " | Verim: %" << (int)eff 
                 << string(cols > 50 ? cols - 50 : 3, ' ') << "║" << endl;
        } else {
            cout << "║ Durum:     " << RED << "🔋 DEŞARJ" << RESET 
                 << string(cols > 30 ? cols - 30 : 3, ' ') << "║" << endl;
        }
        
        cout << "╚" << string(cols - 3, '═') << "╝" << endl;

        // En çok bellek kullanan processler
        if (cols > 60 && iteration % 3 == 0) {
            cout << BOLD_CYAN << "\n╔══ EN ÇOK BELLEK KULLANAN İŞLEMLER ══╗" << RESET << endl;
            auto processes = proc.getTopProcesses(3);
            for (size_t i = 0; i < processes.size(); i++) {
                cout << "║ " << i + 1 << ". " << setw(15) << left << processes[i].name.substr(0, 15) 
                     << " PID:" << setw(6) << processes[i].pid 
                     << " Bellek:" << fixed << setprecision(1) << setw(7) 
                     << processes[i].memSize / 1024.0 << " MiB"
                     << string(cols > 60 ? cols - 60 : 2, ' ') << "║" << endl;
            }
            cout << "╚" << string(cols - 3, '═') << "╝" << endl;
        }

        // Alt bilgi
        cout << endl << string(cols, '─') << endl;
        cout << DIM << " Çıkış: CTRL+C | Yenileme: " << iteration 
             << " | Son güncelleme: " << elapsed << "sn" << RESET;
        cout << string(cols > 50 ? cols - 50 : 2, ' ') << endl;
        
        usleep(850000);
    }
    
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem monitörü kapatıldı. İyi günler!" << RESET << endl;
    
    return 0;
}
