#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>

using namespace std;

// CPU kullanımı için yapı
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

// SICAKLIK OKUMA FONKSİYONU
double getCpuTemperature() {
    // Android cihazlarda sıcaklık genelde bu dosyalardan birindedir
    string paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        "/sys/class/thermal/thermal_zone7/temp" // Bazı cihazlarda ana CPU buradadır
    };

    for (const string& path : paths) {
        ifstream file(path);
        if (file.is_open()) {
            double temp;
            file >> temp;
            // Bazı cihazlar 45000 (mili-derece) verir, bazıları direkt 45.0 verir
            if (temp > 1000) temp /= 1000.0; 
            return temp;
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
        string unit;
        file >> unit;
        if (label == "MemTotal:") totalMem = value;
        if (label == "MemAvailable:") availableMem = value;
    }

    double usedMem = (totalMem - availableMem) / 1024.0;
    cout << "--- BELLEK (RAM) DURUMU ---" << endl;
    cout << "Toplam: " << totalMem / 1024 << " MB" << endl;
    cout << "Kullanılan: " << fixed << setprecision(1) << usedMem << " MB" << endl;
    cout << "Boşta: " << availableMem / 1024 << " MB" << endl;
}

int main() {
    while (true) {
        CpuStats s1 = getCpuStats();
        sleep(1);
        CpuStats s2 = getCpuStats();

        double total1 = s1.idle + s1.iowait + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        double total2 = s2.idle + s2.iowait + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;
        double idle1 = s1.idle + s1.iowait;
        double idle2 = s2.idle + s2.iowait;

        double cpuPercentage = ( (total2 - total1) - (idle2 - idle1) ) / (total2 - total1) * 100.0;
        double currentTemp = getCpuTemperature();

        cout << "\033[2J\033[1;1H"; 
        cout << "========================================" << endl;
        cout << "   ANDROID SISTEM KAYNAK ANALIZI (MIRMEL)" << endl;
        cout << "========================================" << endl;
        cout << "CPU KULLANIMI: %" << fixed << setprecision(2) << cpuPercentage << endl;
        
        if (currentTemp > 0)
            cout << "CPU SICAKLIĞI: " << fixed << setprecision(1) << currentTemp << "°C" << endl;
        else
            cout << "CPU SICAKLIĞI: Okunamadı" << endl;

        cout << "----------------------------------------" << endl;
        printMemoryInfo();
        cout << "========================================" << endl;
        cout << "Çıkmak için CTRL+C tuşlarına basın." << endl;
    }
    return 0;
}
