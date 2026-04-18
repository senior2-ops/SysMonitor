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

using namespace std;

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
#define BOLD_WHITE  "\033[1;37m"
#define BG_BLUE     "\033[44m"

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

class CPUMonitor {
private:
    long long prevIdle = 0, prevTotal = 0;
public:
    double getUsage() {
        ifstream file("/proc/stat");
        string label;
        long long u, n, s, i, io, ir, si;
        if (!(file >> label >> u >> n >> s >> i >> io >> ir >> si)) return 0.0;
        long long idle = i + io;
        long long total = u + n + s + i + io + ir + si;
        double diffTotal = total - prevTotal;
        double diffIdle = idle - prevIdle;
        prevIdle = idle; prevTotal = total;
        return (diffTotal > 0) ? (1.0 - (double)diffIdle / diffTotal) * 100.0 : 0.0;
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

class BatteryMonitor {
public:
    struct BatData {
        long current;
        int capacity;
        double voltage;
        double power;
        int cycles;
        string health;
        bool charging;
    };

    BatData getStats() {
        BatData d = {0, 0, 0.0, 0.0, 0, "Unknown", false};
        ifstream f1("/sys/class/power_supply/battery/current_now"); long c; f1 >> c; d.current = c / 1000;
        ifstream f2("/sys/class/power_supply/battery/capacity"); f2 >> d.capacity;
        ifstream f3("/sys/class/power_supply/battery/voltage_now"); long v; f3 >> v; d.voltage = v / 1000000.0;
        ifstream f4("/sys/class/power_supply/battery/status"); string s; f4 >> s; d.charging = (s == "Charging" || s == "Full");
        ifstream f5("/sys/class/power_supply/battery/health"); f5 >> d.health;
        ifstream f6("/sys/class/power_supply/battery/cycle_count"); f6 >> d.cycles;
        
        d.power = abs(d.voltage * (d.current / 1000.0)); // P = V * I
        return d;
    }
};

class Dashboard {
private:
    CPUMonitor cpu;
    BatteryMonitor bat;
public:
    void render() {
        double cpuPerc = cpu.getUsage();
        double temp = cpu.getTemp();
        auto b = bat.getStats();
        auto [cols, rows] = TerminalUtils::getTerminalSize();

        TerminalUtils::clearScreen();
        // Başlık çubuğu
        cout << BG_BLUE << BOLD_WHITE << string(cols, ' ') << RESET << endl;
        cout << BG_BLUE << BOLD_WHITE << "  SYS-MONITOR PRO v3.5 | ENERJİ ANALİZİ " << string(cols > 40 ? cols - 40 : 0, ' ') << RESET << endl;
        cout << string(cols, '-') << endl;

        // CPU & Termal Bölümü
        cout << BOLD_CYAN << " [SİSTEM PERFORMANSI]" << RESET << endl;
        cout << " CPU Yükü: " << ProgressBar::generate(cpuPerc) << " %" << fixed << setprecision(1) << cpuPerc << endl;
        cout << " Sıcaklık: " << (temp > 45 ? RED : GREEN) << temp << " °C" << RESET << endl;

        // Enerji Analizi Bölümü
        cout << endl << BOLD_YELLOW << " [GÜÇ VE BATARYA ANALİZİ]" << RESET << endl;
        cout << " Kapasite: " << ProgressBar::generate(b.capacity) << " %" << b.capacity << endl;
        cout << " Akım/Güç: " << (b.current >= 0 ? GREEN : RED) << b.current << " mA" << RESET 
             << " | " << BOLD << fixed << setprecision(2) << b.power << " Watt" << RESET << endl;
        cout << " Voltaj:    " << CYAN << b.voltage << " V" << RESET << endl;
        
        // Pil Sağlığı ve Ömrü
        cout << endl << BOLD_MAGENTA << " [PİL SAĞLIK VE ÖMÜR]" << RESET << endl;
        cout << " Sağlık:   " << (b.health == "Good" ? GREEN : YELLOW) << b.health << RESET << endl;
        cout << " Döngü:    " << WHITE << b.cycles << " Tam Döngü" << RESET << endl;

        if (b.charging) {
            double eff = 100.0 - (cpuPerc * 0.4) - (temp > 38 ? (temp - 38) * 2 : 0);
            cout << " Durum:    " << GREEN << "ŞARJ OLUYOR (Verim: %" << (int)(eff > 0 ? eff : 0) << ")" << RESET << endl;
        } else {
            cout << " Durum:    " << RED << "PİL HARCANIYOR (Deşarj)" << RESET << endl;
        }

        cout << endl << string(cols, '-') << endl;
        cout << DIM << " Çıkış: CTRL+C | Mühendislik Modu Aktif" << RESET << endl;
    }
};

int main() {
    signal(SIGINT, signalHandler);
    TerminalUtils::hideCursor();
    Dashboard ds;
    while (g_running) {
        ds.render();
        usleep(850000); // 0.85 saniye yenileme
    }
    TerminalUtils::showCursor();
    TerminalUtils::clearScreen();
    cout << GREEN << "Sistem izleme başarıyla sonlandırıldı." << RESET << endl;
    return 0;
}
