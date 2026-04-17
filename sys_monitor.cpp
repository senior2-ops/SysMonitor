#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>

using namespace std;

// CPU kullanımını hesaplamak için veri yapısı
struct CpuStats {
    long long user, nice, system, idle, iowait, irq, softirq;
};

CpuStats getCpuStats() {
    ifstream file("/proc/stat");
    string cpu;
    CpuStats stats;
    file >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle >> stats.iowait >> stats.irq >> stats.softirq;
    return stats;
}

void printMemoryInfo() {
    ifstream file("/proc/meminfo");
    string label;
    long value;
    long totalMem = 0, freeMem = 0, availableMem = 0;

    while (file >> label >> value) {
        string unit;
        file >> unit;
        if (label == "MemTotal:") totalMem = value;
        if (label == "MemFree:") freeMem = value;
        if (label == "MemAvailable:") availableMem = value;
    }

    double usedMem = (totalMem - availableMem) / 1024.0;
    cout << "--- BELLEK (RAM) DURUMU ---" << endl;
    cout << "Toplam: " << totalMem / 1024 << " MB" << endl;
    cout << "Kullanılan: " << fixed << setprecision(1) << usedMem << " MB" << endl;
    cout << "Boşta: " << availableMem / 1024 << " MB" << endl;
}

int main() {
    cout << "\033[2J\033[1;1H"; 
    cout << "NetProbe SysMonitor v1.0 başlatılıyor..." << endl;

    while (true) {
        CpuStats s1 = getCpuStats();
        sleep(1);
        CpuStats s2 = getCpuStats();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long nonIdle1 = s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long nonIdle2 = s2.user + s2.nice + s2.system + s2.irq + s2.softirq;

        double total1 = idle1 + nonIdle1;
        double total2 = idle2 + nonIdle2;

        double cpuPercentage = ( (total2 - total1) - (idle2 - idle1) ) / (total2 - total1) * 100.0;

        cout << "\033[2J\033[1;1H"; // Her saniye ekranı yenile
        cout << "========================================" << endl;
        cout << "   ANDROID SISTEM KAYNAK ANALIZI       " << endl;
        cout << "========================================" << endl;
        cout << "CPU KULLANIMI: %" << fixed << setprecision(2) << cpuPercentage << endl;
        cout << "----------------------------------------" << endl;
        printMemoryInfo();
        cout << "========================================" << endl;
        cout << "Çıkmak için CTRL+C tuşlarına basın." << endl;
    }
    return 0;
}
