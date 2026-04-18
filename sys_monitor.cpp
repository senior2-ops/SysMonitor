#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <iomanip>
#include <algorithm>
#include <ctime>

using namespace std;

// --- PROFESYONEL RENK VE STİL PALETİ ---
const string CLEAR   = "\033[2J\033[1;1H";
const string RESET   = "\033[0m";
const string BOLD    = "\033[1m";
const string GREEN   = "\033[32m";
const string YELLOW  = "\033[33m";
const string RED     = "\033[31m";
const string CYAN    = "\033[36m";
const string MAGENTA = "\033[35m";
const string BG_RED  = "\033[41m\033[37m";

struct CpuStats {
    long long user, nice, system, idle, iowait, irq, softirq;
};

// --- MÜHENDİSLİK FONKSİYONLARI ---

// Gelişmiş Grafik Çubuğu
string drawBar(double perc, int width = 15) {
    string bar = "[";
    int pos = width * (perc / 100.0);
    string color = (perc < 50) ? GREEN : (perc < 80 ? YELLOW : RED);
    
    for (int i = 0; i < width; ++i) {
        if (i < pos) bar += color + "■" + RESET;
        else bar += " ";
    }
    bar += "]";
    return bar;
}

// Çekirdek İstatistiklerini Okur
CpuStats fetchCpu() {
    ifstream file("/proc/stat");
    string cpu_label;
    CpuStats s = {0};
    if (file >> cpu_label) {
        file >> s.user >> s.nice >> s.system >> s.idle >> s.iowait >> s.irq >> s.softirq;
    }
    return s;
}

// Akıllı Sensör Tarayıcı (Listendeki tüm sensörleri destekler)
double getTemp() {
    const string targets[] = {"cpu-0-0-usr", "cpu-1-0-usr", "cpu-thermal", "tsens_tz_sensor0", "battery"};
    for (const string& type_name : targets) {
        for (int i = 0; i < 100; i++) {
            string path = "/sys/class/thermal/thermal_zone" + to_string(i);
            ifstream t_file(path + "/type");
            string t_str;
            if (t_file >> t_str && t_str == type_name) {
                ifstream temp_val(path + "/temp");
                double val;
                if (temp_val >> val) return (val > 1000) ? val / 1000.0 : val;
            }
        }
    }
    return 0.0;
}

// Enerji Akımı (mA)
long getAmper() {
    ifstream bFile("/sys/class/power_supply/battery/current_now");
    long curr = 0;
    if (bFile >> curr) return curr / 1000;
    return 0;
}

/
void showHeader() {
    cout << CYAN << "┌──────────────────────────────────────┐" << endl;
    cout << "│      " << BOLD << "SYS-MONITOR" << RESET << CYAN << "        │" << endl;
    cout << "│           " << MAGENTA << "MIRMEL" << RESET << CYAN << "                │" << endl;
    cout << "└──────────────────────────────────────┘" << RESET << endl;
}

int main() {
    while (true) {
        // CPU Delta Hesaplama
        CpuStats s1 = fetchCpu();
        usleep(700000); // 0.8s Örnekleme
        CpuStats s2 = fetchCpu();

        long long idle1 = s1.idle + s1.iowait;
        long long idle2 = s2.idle + s2.iowait;
        long long total1 = idle1 + s1.user + s1.nice + s1.system + s1.irq + s1.softirq;
        long long total2 = idle2 + s2.user + s2.nice + s2.system + s2.irq + s2.softirq;
        
        double cpuPerc = (total2 > total1) ? (double)((total2 - total1) - (idle2 - idle1)) / (total2 - total1) * 100.0 : 0.0;

        // RAM Analizi
        ifstream memFile("/proc/meminfo");
        long totalK = 0, availK = 0;
        string key;
        while (memFile >> key) {
            if (key == "MemTotal:") memFile >> totalK;
            else if (key == "MemAvailable:") memFile >> availK;
            else { string dummy; memFile >> dummy; }
        }
        double totalM = totalK / 1024.0;
        double usedM = (totalK - availK) / 1024.0;
        double ramPerc = (totalM > 0) ? (usedM / totalM) * 100.0 : 0.0;

        double temp = getTemp();
        long curr = getAmper();

        
        cout << CLEAR;
        
        
        if (temp > 48.0) {
            cout << BG_RED << BOLD << " !!! YUKSEK ISI: SARJ HIZI DUSEBILIR !!! " << RESET << endl << endl;
        }
        
        showHeader();
        
        cout << BOLD << " [KAYNAK KULLANIMI] " << RESET << endl;
        cout << " CPU  " << drawBar(cpuPerc) << " %" << fixed << setprecision(1) << cpuPerc << endl;
        cout << " RAM  " << drawBar(ramPerc) << " %" << setprecision(1) << ramPerc << endl;
        cout << " Detay: " << (int)usedM << " MB / " << (int)totalM << " MB" << endl;
        
        cout << endl << BOLD << " [ENERJI & TERMAL] " << RESET << endl;
        cout << " Sicaklik: " << (temp > 40 ? RED : GREEN) << temp << " °C" << RESET << endl;
        cout << " Akim:     " << (curr > 0 ? GREEN : RED) << curr << " mA" << RESET << endl;
        

        if (curr > 0) {
            double efficiency = 100.0 - (cpuPerc * 0.4) - (temp > 35 ? (temp - 35) * 3 : 0);
            if (efficiency < 0) efficiency = 0;
            cout << " Verim:    " << YELLOW << "%" << (int)efficiency << RESET << " (Optimize Ediliyor)" << endl;
        } else {
            cout << " Durum:    " << RED << "Pil Harcaniyor" << RESET << endl;
        }

        cout << endl << CYAN << "────────────────────────────────────────" << endl;
        cout << RESET << " Durdurmak icin " << BOLD << "CTRL + C" << RESET << endl;
    }
    return 0;
}
