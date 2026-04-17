#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>

using namespace std;

struct CpuStats {
    long long user, nice, system, idle, iowait, irq, softirq;
};

CpuStats getCpuStats() {
    ifstream file("/proc/stat");
    string cpu;
    CpuStats stats = {0};
    if (file >> cpu) {
        file >> stats.user >> stats.nice >> stats.system >> stats.idle >> stats.iowait >> stats.irq >> stats.softirq;
    }
    return stats;
}

// Çekirdek Frekansını Okur (MHz cinsinden)
long getCpuFreq() {
    ifstream file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    long freq;
    if (file >> freq) return freq / 1000; // kHz to MHz
    return 0;
}

// Batarya Akımını Okur (mA cinsinden)
long getBatteryCurrent() {
    ifstream file("/sys/class/power_supply/battery/current_now");
    long current;
    if (file >> current) return current / 1000; // microAmps to milliAmps
    return 0;
}

// Batarya Sağlığı/Durumu
string getBatteryStatus() {
    ifstream file("/sys/class/power_supply/battery/status");
    string status;
    if (file >> status) return status;
    return "Bilinmiyor";
}

double getCpuTemperature() {
    for (int i = 0; i < 100; i++) {
        string baseDir = "/sys/class/thermal/thermal_zone" + to_string(i);
        string typePath = baseDir + "/type";
        ifstream typeFile(typePath);
        string type;
        if (typeFile >> type) {
            if (type == "cpu-0-0-usr" || type == "cpu-1-0-usr" || type == "cpu-thermal") {
                string tempPath = baseDir + "/temp";
                ifstream tempFile(tempPath);
                double temp;
                if (tempFile >> temp) return (temp > 1000) ? temp / 1000.0 : temp;
            }
        }
    }
    return 0.0;
}

void printMemoryInfo() {
    ifstream file("/proc/meminfo");
    string label;
    long value;
    long totalMem = 0, availableMem = 0;
    while (file >> label >> value) {
        string unit; file >> unit;
        if (label == "MemTotal:") totalMem = value;
        if (label == "MemAvailable:") availableMem = value;
    }
    double usedMem = (totalMem - availableMem) / 1024.0;
    cout << "--- BELLEK (RAM) DURUMU ---" << endl;
    cout << "Toplam:    " << totalMem / 1024 << " MB" << endl;
    cout << "Kullanılan: " << fixed << setprecision(1) << usedMem << " MB" << endl;
    cout << "Boşta:     " << availableMem / 1024 << " MB" << endl;
}

int main() {
    while (true) {
        CpuStats s1 = getCpuStats();
        usleep(500000);
        CpuStats s2 = getCpuStats();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long total1 = idle1 + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long total2 = idle2 + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;

        double diffTotal = (double)(total2 - total1);
        double diffIdle = (double)(idle2 - idle1);
        double cpuPercentage = (diffTotal > 0) ? (diffTotal - diffIdle) / diffTotal * 100.0 : 0.0;

        cout << "\033[2J\033[1;1H"; 
        cout << "========================================" << endl;
        cout << "   ANDROID SISTEM VE ENERJI ANALIZI     " << endl;
        cout << "               (MIRMEL)                 " << endl;
        cout << "========================================" << endl;
        
        cout << "CPU KULLANIMI: %" << fixed << setprecision(2) << cpuPercentage << endl;
        cout << "CPU FREKANSI:  " << getCpuFreq() << " MHz" << endl;
        cout << "CPU SICAKLIĞI: " << fixed << setprecision(1) << getCpuTemperature() << "°C" << endl;
        
        cout << "----------------------------------------" << endl;
        cout << "BATARYA DURUMU: " << getBatteryStatus() << endl;
        cout << "ANLIK AKIM:     " << getBatteryCurrent() << " mA" << endl;
        
        cout << "----------------------------------------" << endl;
        printMemoryInfo();
        cout << "========================================" << endl;
        cout << "Durdurmak için: CTRL + C" << endl;
    }
    return 0;
}
