#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>
#include <filesystem>

using namespace std;

// CPU kullanım verileri için yapı
struct CpuStats {
    long long user, nice, system, idle, iowait, irq, softirq;
};

// Çekirdekten CPU yükünü okur
CpuStats getCpuStats() {
    ifstream file("/proc/stat");
    string cpu;
    CpuStats stats = {0};
    if (file >> cpu) {
        file >> stats.user >> stats.nice >> stats.system >> stats.idle >> stats.iowait >> stats.irq >> stats.softirq;
    }
    return stats;
}

// Senin cihazındaki özel termal bölgeleri tarayan fonksiyon
double getCpuTemperature() {
    // 0'dan 100'e kadar olan tüm thermal_zone klasörlerini kontrol et
    for (int i = 0; i < 100; i++) {
        string baseDir = "/sys/class/thermal/thermal_zone" + to_string(i);
        string typePath = baseDir + "/type";
        
        ifstream typeFile(typePath);
        string type;
        if (typeFile >> type) {
            // Senin listendeki kritik işlemci isimlerini kontrol ediyoruz
            if (type == "cpu-0-0-usr" || type == "cpu-1-0-usr" || type == "cpu-thermal" || type == "tsens_tz_sensor0") {
                string tempPath = baseDir + "/temp";
                ifstream tempFile(tempPath);
                double temp;
                if (tempFile >> temp) {
                    // Mili-derece gelirse (örn: 45000) normal dereceye çevir
                    return (temp > 1000) ? temp / 1000.0 : temp;
                }
            }
        }
    }
    return 0.0;
}

// RAM bilgilerini /proc/meminfo'dan çeker
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
    cout << "Toplam:    " << totalMem / 1024 << " MB" << endl;
    cout << "Kullanılan: " << fixed << setprecision(1) << usedMem << " MB" << endl;
    cout << "Boşta:     " << availableMem / 1024 << " MB" << endl;
}

int main() {
    // Terminal ekranını temizle
    cout << "\033[2J\033[1;1H";
    cout << "SysMonitor v1.2 - Android Mühendislik Aracı Başlatılıyor..." << endl;
    sleep(1);

    while (true) {
        CpuStats s1 = getCpuStats();
        sleep(1); // Hassas ölçüm için 1 saniye bekle
        CpuStats s2 = getCpuStats();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long total1 = idle1 + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long total2 = idle2 + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;

        double cpuPercentage = (double)((total2 - total1) - (idle2 - idle1)) / (total2 - total1) * 100.0;
        double currentTemp = getCpuTemperature();

        // Ekranı her saniye güncelle
        cout << "\033[2J\033[1;1H"; 
        cout << "========================================" << endl;
        cout << "   ANDROID SISTEM KAYNAK ANALIZI        " << endl;
        cout << "========================================" << endl;
        
        cout << "CPU KULLANIMI: %" << fixed << setprecision(2) << cpuPercentage << endl;
        
        if (currentTemp > 0)
            cout << "CPU SICAKLIĞI: " << fixed << setprecision(1) << currentTemp << "°C" << endl;
        else
            cout << "CPU SICAKLIĞI: Aranıyor..." << endl;

        cout << "----------------------------------------" << endl;
        printMemoryInfo();
        cout << "========================================" << endl;
        cout << "Durdurmak için: CTRL + C" << endl;
    }
    return 0;
}

