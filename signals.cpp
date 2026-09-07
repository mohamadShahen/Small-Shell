#include <iostream>
#include <signal.h>
#include <unistd.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;

    SmallShell& smash = SmallShell::getInstance();

    if (smash.fgPid > 0) {
        if (kill(smash.fgPid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }

        cout << "smash: process " << smash.fgPid << " was killed" << endl;
        smash.fgPid = -1;
    }
}