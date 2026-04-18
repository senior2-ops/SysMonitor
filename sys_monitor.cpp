#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>

using namespace std;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define BOLDRED "\033[1;31m"

struct CpuStats {
    long long user, nice, system, idle, iowait, irq, softirq;
};

// --- SİSTEM FONKSİYONLARI ---
CpuStats getCpuStats() {
    ifstream file("/proc/stat");
    string cpu;
    CpuStats stats = {0};
    if (file >> cpu) {
        file >> stats.user >> stats.nice >> stats.system >> stats.idle >> stats.iowait >> stats.irq >> stats.softirq;
    }
    return stats;
}

string getProgressBar(double percentage, int width = 15) {
    string bar = "[";
    int pos = width * (percentage / 100.0);
    for (int i = 0; i < width; ++i) {
        if (i < pos) bar += "#";
        else bar += "-";
    }
    bar += "]";
    return bar;
}

double getCpuTemperature() {
    for (int i = 0; i < 80; i++) {
        string baseDir = "/sys/class/thermal/thermal_zone" + to_string(i);
        ifstream typeFile(baseDir + "/type");
        string type;
        if (typeFile >> type && (type == "cpu-0-0-usr" || type == "cpu-1-0-usr" || type == "cpu-thermal")) {
            ifstream tempFile(baseDir + "/temp");
            double t;
            if (tempFile >> t) return (t > 1000) ? t / 1000.0 : t;
        }
    }
    return 0.0;
}

long getBatteryCurrent() {
    ifstream file("/sys/class/power_supply/battery/current_now");
    long current;
    if (file >> current) return current / 1000;
    return 0;
}

int main() {
    while (true) {
        // CPU Ölçümü
        CpuStats s1 = getCpuStats();
        usleep(700000); // 0.7 saniye örnekleme hızı
        CpuStats s2 = getCpuStats();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long total1 = idle1 + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long total2 = idle2 + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;

        double diffTotal = (double)(total2 - total1);
        double diffIdle = (double)(idle2 - idle1);
        double cpuPerc = (diffTotal > 0) ? (diffTotal - diffIdle) / diffTotal * 100.0 : 0.0;

        // Diğer Veriler
        long current = getBatteryCurrent();
        double temp = getCpuTemperature();

        // RAM Detayları
        ifstream memFile("/proc/meminfo");
        string label; long totalM = 0, availM = 0;
        while (memFile >> label) {
            if (label == "MemTotal:") memFile >> totalM;
            else if (label == "MemAvailable:") memFile >> availM;
            else { string dummy; memFile >> dummy; }
        }
        double ramUsed = (totalM - availM) / 1024.0;
        double ramTotal = totalM / 1024.0;
        double ramPerc = (ramTotal > 0) ? (ramUsed / ramTotal) * 100.0 : 0.0;

        // --- EKRAN ÇIKTISI ---
        cout << "\033[2J\033[1;1H"; 
        
        // KRİTİK ISI ALARMI (Öneri 2)
        if (temp > 42.0) {
            cout << BOLDRED << "!!! KRITIK SICAKLIK UYARISI !!!" << RESET << endl;
        }

        cout << CYAN << "========================================" << RESET << endl;
        cout << YELLOW << "    SYS-MONITOR v2.5 | LAB EDITION" << RESET << endl;
        cout << YELLOW << "                MIRMEL" << RESET << endl;
        cout << CYAN << "========================================" << RESET << endl;
        
        // İşlemci Bölümü
        cout << "CPU YUKU:    " << getProgressBar(cpuPerc) << " %" << fixed << setprecision(1) << cpuPerc << endl;
        cout << "CPU ISI:     " << (temp > 42 ? BOLDRED : GREEN) << temp << "°C" << RESET << endl;
        
        cout << "----------------------------------------" << endl;
        
        // Enerji ve Verimlilik (Öneri 3)
        cout << "ENERJI AKIMI: " << (current >= 0 ? GREEN : RED) << current << " mA" << RESET << endl;
        
        if (current > 0) {
            double efficiency = 100.0 - (cpuPerc * 0.5); // Basit mühendislik katsayısı
            cout << GREEN << ">> DURUM: SARJ OLUYOR" << RESET << endl;
            cout << "VERIMLILIK SKORU: %" << setprecision(0) << (efficiency > 0 ? efficiency : 0) << endl;
        } else {
            cout << RED << ">> DURUM: BATARYA KULLANIMDA" << RESET << endl;
        }

        cout << "----------------------------------------" << endl;
        
        // Bellek Bölümü (Eksiksiz Detay)
        cout << "RAM KULLANIMI: " << getProgressBar(ramPerc) << " %" << setprecision(1) << ramPerc << endl;
        cout << "TOPLAM RAM:    " << (int)ramTotal << " MB" << endl;
        cout << "KULLANILAN:    " << (int)ramUsed << " MB" << endl;
        cout << "BOSTA RAM:     " << (int)(availM / 1024.0) << " MB" << endl;

        cout << CYAN << "========================================" << RESET << endl;
        cout << "Kapatmak için: CTRL + C" << endl;
    }
    return 0;
}
