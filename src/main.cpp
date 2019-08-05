#include <QApplication>
#include "mainwindow.h"
#include "debugview.h"
#include "Cpu6502.hpp"
#include "Ppu.hpp"
#include "NES.hpp"

int main(int argc, char *argv[]) {
    Ppu ppu;
    Cpu6502 cpu;
    NES nes(cpu, ppu);
    QApplication a(argc, argv);
    //MainWindow w;
    //w.show();
    DebugView d(nes);
    d.show();
    return a.exec();
}
