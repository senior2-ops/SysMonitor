#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>

using namespace std;

// Renk Kodları
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"

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

string getProgressBar(double percentage, int width = 20) {
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
    for (int i = 0; i < 60; i++) {
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
        CpuStats s1 = getCpuStats();
        usleep(500000);
        CpuStats s2 = getCpuStats();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long total1 = idle1 + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long total2 = idle2 + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;

        double diffTotal = (double)(total2 - total1);
        double diffIdle = (double)(idle2 - idle1);
        double cpuPerc = (diffTotal > 0) ? (diffTotal - diffIdle) / diffTotal * 100.0 : 0.0;

        long current = getBatteryCurrent();
        double temp = getCpuTemperature();

        // EKRAN ÇIKTISI
        cout << "\033[2J\033[1;1H"; 
        cout << CYAN << "========================================" << RESET << endl;
        cout << YELLOW << "   SYS-MONITOR v2.0 | OPTIMIZED MODE" << RESET << endl;
        cout << YELLOW << "                  MIRMEL            " << RESET << endl;
        cout << CYAN << "========================================" << RESET << endl;
        
        // CPU Bölümü
        cout << "CPU YÜKÜ:   " << getProgressBar(cpuPerc) << " %" << fixed << setprecision(1) << cpuPerc << endl;
        cout << "CPU ISI:    " << (temp > 45 ? RED : GREEN) << temp << "°C" << RESET << endl;
        
        cout << "----------------------------------------" << endl;
        
        // Enerji Bölümü
        cout << "ENERJİ AKIMI: " << (current >= 0 ? GREEN : RED) << current << " mA" << RESET << endl;
        
        if (current > 0) {
            cout << GREEN << ">> DURUM: VERIMLI SARJ OLUYOR" << RESET << endl;
            if (cpuPerc > 15) cout << YELLOW << "!! UYARI: YUKSEK CPU SARJI YAVASLATIR" << RESET << endl;
        } else {
            cout << RED << ">> DURUM: DEŞARJ OLUYOR (PIL HARCANIYOR)" << RESET << endl;
        }

        cout << "----------------------------------------" << endl;
        
        // Bellek (RAM)
        ifstream memFile("/proc/meminfo");
        string label; long totalM, availM;
        memFile >> label >> totalM >> label >> label >> availM; // Basit okuma
        double ramPerc = 100.0 * (totalM - availM) / totalM;
        cout << "RAM KULLANIMI: " << getProgressBar(ramPerc) << " %" << setprecision(1) << ramPerc << endl;

        cout << CYAN << "========================================" << RESET << endl;
        cout << "Kapatmak için: CTRL + C" << endl;
    }
    return 0;
}
