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
#include <chrono>
#include <thread>
#include <dirent.h>

using namespace std;
using namespace chrono;

// --- RENK VE GÖRSEL TANIMLAMALAR ---
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define BOLD_RED    "\033[1;31m"
#define BOLD_GREEN  "\033[1;32m"
#define BOLD_YELLOW "\033[1;33m"
#define BOLD_BLUE   "\033[1;34m"
#define BOLD_CYAN   "\033[1;36m"
#define BOLD_WHITE  "\033[1;37m"
#define BG_BLUE     "\033[44m"

volatile sig_atomic_t g_running = 1;
void signalHandler(int signum) { g_running = 0; }

// --- MÜHENDİSLİK ARAÇLARI ---
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
    static string generate(double percentage, int width = 20) {
        string bar = "[";
        int pos = (int)(width * (percentage / 100.0));
        for (int i = 0; i < width; ++i) {
            if (i < pos) {
                if (percentage >= 85) bar += RED "#" RESET;
                else if (percentage >= 60) bar += YELLOW "#" RESET;
                else bar += GREEN "#" RESET;
            } else bar += DIM "." RESET;
        }
        return bar + "]";
    }
};

// --- İZLEME MODÜLLERİ ---
class CPUMonitor {
private:
    long long prevIdle = 0, prevTotal = 0;
public:
    double getUsage() {
        ifstream file("/proc/stat");
        string label;
        long long u, n, s, i, io, ir, si;
        file >> label >> u >> n >> s >> i >> io >> ir >> si;
        long long idle = i + io;
        long long total = u + n + s + i + io + ir + si;
        double diffTotal = total - prevTotal;
        double diffIdle = idle - prevIdle;
        prevIdle = idle; prevTotal = total;
        return (diffTotal > 0) ? (1.0 - (diffIdle / diffTotal)) * 100.0 : 0.0;
    }
    double getTemp() {
        const string targets[] = {"cpu-thermal", "battery", "cpu-0-0-usr"};
        for (auto& t : targets) {
            for (int i = 0; i < 60; i++) {
                string path = "/sys/class/thermal/thermal_zone" + to_string(i);
                ifstream f(path + "/type"); string type;
                if (f >> type && type == t) {
                    ifstream tf(path + "/temp"); double v;
                    if (tf >> v) return (v > 1000 ? v / 1000.0 : v);
                }
            }
        }
        return 0.0;
    }
};

class MemoryMonitor {
public:
    void getStats(long& total, long& used, double& perc) {
        ifstream file("/proc/meminfo");
        long avail = 0; string key;
        while (file >> key) {
            if (key == "MemTotal:") file >> total;
            else if (key == "MemAvailable:") file >> avail;
            else { string d; file >> d; }
        }
        total /= 1024; avail /= 1024;
        used = total - avail;
        perc = (total > 0) ? (double)used / total * 100.0 : 0.0;
    }
};

class BatteryMonitor {
public:
    void getStats(long& curr, int& cap, bool& charging) {
        ifstream f1("/sys/class/power_supply/battery/current_now"); f1 >> curr; curr /= 1000;
        ifstream f2("/sys/class/power_supply/battery/capacity"); f2 >> cap;
        ifstream f3("/sys/class/power_supply/battery/status"); string s; f3 >> s;
        charging = (s == "Charging" || s == "Full");
    }
};

// --- ANA PANEL ---
class Dashboard {
private:
    CPUMonitor cpu;
    MemoryMonitor mem;
    BatteryMonitor bat;
public:
    void render() {
        double cpuPerc = cpu.getUsage();
        double temp = cpu.getTemp();
        long mTotal, mUsed; double mPerc; mem.getStats(mTotal, mUsed, mPerc);
        long bCurr; int bCap; bool bChar; bat.getStats(bCurr, bCap, bChar);
        auto [cols, rows] = TerminalUtils::getTerminalSize();

        TerminalUtils::clearScreen();
        cout << BG_BLUE << BOLD_WHITE << string(cols, ' ') << RESET << endl;
        cout << BG_BLUE << BOLD_WHITE << "  SYS-MONITOR MIRMEL  " << string(cols - 39, ' ') << RESET << endl;
        cout << string(cols, '-') << endl;

        cout << BOLD_CYAN << " [SİSTEM KAYNAKLARI]" << RESET << endl;
        cout << " CPU: " << ProgressBar::generate(cpuPerc) << " %" << fixed << setprecision(1) << cpuPerc << endl;
        cout << " RAM: " << ProgressBar::generate(mPerc) << " " << mUsed << "/" << mTotal << " MB" << endl;
        cout << " Isı: " << (temp > 45 ? RED : GREEN) << temp << " °C" << RESET << endl;

        cout << endl << BOLD_YELLOW << " [ENERJİ ANALİZİ]" << RESET << endl;
        cout << " Pil: " << ProgressBar::generate(bCap) << " %" << bCap << endl;
        cout << " Akım: " << (bCurr >= 0 ? GREEN : RED) << bCurr << " mA" << RESET << endl;
        
        if (bChar) {
            double eff = 100.0 - (cpuPerc * 0.4) - (temp > 38 ? (temp - 38) * 2 : 0);
            cout << " Durum: " << GREEN << "ŞARJ OLUYOR" << RESET << " | Verim: %" << (int)(eff > 0 ? eff : 0) << endl;
        } else {
            cout << " Durum: " << RED << "DEŞARJ (PİL HARCANIYOR)" << RESET << endl;
        }

        cout << endl << string(cols, '-') << endl;
        cout << DIM << " Çıkış: CTRL+C | Yenileme: 0.8s" << RESET << endl;
    }
};

int main() {
    signal(SIGINT, signalHandler);
    TerminalUtils::hideCursor();
    Dashboard ds;
    while (g_running) {
        ds.render();
        usleep(800000);
    }
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem izleme başarıyla sonlandırıldı." << RESET << endl;
    return 0;
}
