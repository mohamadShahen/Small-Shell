#include <sys/utsname.h>
#include <fstream>
#include <ctime>
#include "Commands.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <pwd.h>

#include <sys/types.h>
extern char **environ;
static long calculateDiskUsage(const std::string& path) {
    struct stat st;

    if (lstat(path.c_str(), &st) == -1) {
        return 0;
    }

    // Do not follow symbolic links
    if (S_ISLNK(st.st_mode)) {
        return st.st_blocks / 2;
    }

    long total = st.st_blocks / 2; 
    // st_blocks is in 512-byte blocks, so divide by 2 to get KB

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(path.c_str());

        if (dir == nullptr) {
            return total;
        }

        struct dirent* entry;

        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;

            if (name == "." || name == "..") {
                continue;
            }

            std::string fullPath = path + "/" + name;
            total += calculateDiskUsage(fullPath);
        }

        closedir(dir);
    }

    return total;
}
string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 
void SysInfoCommand::execute() {
    struct utsname uts;

    if (uname(&uts) == -1) {
        perror("smash error: uname failed");
        return;
    }

    char hostname[COMMAND_MAX_LENGTH];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("smash error: gethostname failed");
        return;
    }

    std::cout << "System: " << uts.sysname << std::endl;
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Kernel: " << uts.release << std::endl;
    std::cout << "Architecture: " << uts.machine << std::endl;

    std::ifstream statFile("/proc/stat");
    std::string word;
    long bootTime = 0;

    while (statFile >> word) {
        if (word == "btime") {
            statFile >> bootTime;
            break;
        }

        std::string restOfLine;
        std::getline(statFile, restOfLine);
    }

    if (bootTime == 0) {
        std::cerr << "smash error: failed to read boot time" << std::endl;
        return;
    }

    time_t boot = static_cast<time_t>(bootTime);
    struct tm* timeInfo = localtime(&boot);

    char timeBuffer[COMMAND_MAX_LENGTH];

    if (strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo) == 0) {
        std::cerr << "smash error: failed to format boot time" << std::endl;
        return;
    }

    std::cout << "Boot Time: " << timeBuffer << std::endl;
}
void RedirectionCommand::execute() {
    std::string line = _trim(std::string(cmd_line));

    bool append = false;
    size_t pos = line.find(">>");

    if (pos != std::string::npos) {
        append = true;
    } else {
        pos = line.find(">");
    }

    if (pos == std::string::npos) {
        return;
    }

    std::string commandPart = _trim(line.substr(0, pos));

    std::string filePart;
    if (append) {
        filePart = _trim(line.substr(pos + 2));
    } else {
        filePart = _trim(line.substr(pos + 1));
    }

    if (filePart.empty()) {
        return;
    }

    int flags;

    if (append) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    }

    int fd = open(filePart.c_str(), flags, 0655);

    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    int stdout_copy = dup(STDOUT_FILENO);

    if (stdout_copy == -1) {
        perror("smash error: dup failed");
        close(fd);
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
        close(fd);
        close(stdout_copy);
        return;
    }

    close(fd);

    SmallShell::getInstance().executeCommand(commandPart.c_str());

    if (dup2(stdout_copy, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
        close(stdout_copy);
        return;
    }

    close(stdout_copy);
}
SmallShell::SmallShell() {
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}
void UnSetEnvCommand::execute() {
    char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    if (argn < 2) {
        std::cerr << "smash error: unsetenv: not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < argn; i++) {
        std::string varName = args[i];
        bool found = false;

        for (int j = 0; environ[j] != nullptr; j++) {
            std::string envEntry = environ[j];

            size_t eqPos = envEntry.find('=');
            std::string currentName = envEntry.substr(0, eqPos);

            if (currentName == varName) {
                found = true;

                for (int k = j; environ[k] != nullptr; k++) {
                    environ[k] = environ[k + 1];
                }

                break;
            }
        }

        if (!found) {
            std::cerr << "smash error: unsetenv: "
                      << varName
                      << " does not exist"
                      << std::endl;
            return;
        }
    }
}
void AliasCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();

    std::string line = _trim(std::string(cmd_line));

    // alias with no arguments: print all aliases
    if (line == "alias") {
        for (const auto& a : smash.aliases) {
            std::cout << a.first << "='" << a.second << "'" << std::endl;
        }
        return;
    }

    // remove "alias"
    std::string rest = _trim(line.substr(5));

    size_t eqPos = rest.find('=');

    if (eqPos == std::string::npos) {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    std::string name = _trim(rest.substr(0, eqPos));
    std::string commandPart = _trim(rest.substr(eqPos + 1));

    // name must be valid
    if (!isValidAliasName(name)) {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    // command must be inside single quotes
    if (commandPart.length() < 2 ||
        commandPart.front() != '\'' ||
        commandPart.back() != '\'') {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    std::string realCommand = commandPart.substr(1, commandPart.length() - 2);

    // reserved command
    if (isReservedKeyword(name)) {
        std::cerr << "smash error: alias: "
                  << name
                  << " already exists or is a reserved command"
                  << std::endl;
        return;
    }

    // alias already exists
    for (const auto& a : smash.aliases) {
        if (a.first == name) {
            std::cerr << "smash error: alias: "
                      << name
                      << " already exists or is a reserved command"
                      << std::endl;
            return;
        }
    }

    smash.aliases.push_back({name, realCommand});
}
void UnAliasCommand::execute() {
    char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    if (argn < 2) {
        std::cerr << "smash error: unalias: not enough arguments" << std::endl;
        return;
    }

    SmallShell& smash = SmallShell::getInstance();

    for (int i = 1; i < argn; i++) {
        std::string aliasName = args[i];
        bool found = false;

        for (auto it = smash.aliases.begin(); it != smash.aliases.end(); ++it) {
            if (it->first == aliasName) {
                smash.aliases.erase(it);
                found = true;
                break;
            }
        }

        if (!found) {
            std::cerr << "smash error: unalias: "
                      << aliasName
                      << " alias does not exist"
                      << std::endl;
            return;
        }
    }
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/void WhoAmICommand::execute() {
    uid_t uid = getuid();
    gid_t gid = getgid();

    struct passwd* pw = getpwuid(uid);

    if (pw == nullptr) {
        perror("smash error: getpwuid failed");
        return;
    }

    std::cout << pw->pw_name << std::endl;
    std::cout << uid << std::endl;
    std::cout << gid << std::endl;
    std::cout << pw->pw_dir << std::endl;
}

void DiskUsageCommand::execute() {
    char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    if (argn > 2) {
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }

    std::string path;

    if (argn == 1) {
        path = ".";
    } else {
        path = args[1];
    }

    long totalKB = calculateDiskUsage(path);

    std::cout << "Total disk usage: " << totalKB << " KB" << std::endl;
}
void ExternalCommand::execute() {
    std::string line = _trim(std::string(cmd_line));

    bool isBackground = _isBackgroundCommand(line.c_str());

    std::string originalCmd = line;

    if (isBackground) {
        char cmdCopy[COMMAND_MAX_LENGTH];
        strcpy(cmdCopy, line.c_str());
        _removeBackgroundSign(cmdCopy);
        line = _trim(std::string(cmdCopy));
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("smash error: fork failed");
        return;
    }

    if (pid == 0) {
        // child process
        setpgrp();

        bool isComplex = false;

        if (line.find('*') != std::string::npos ||
            line.find('?') != std::string::npos) {
            isComplex = true;
        }

        if (isComplex) {
            char* bashArgs[4];
            bashArgs[0] = (char*)"/bin/bash";
            bashArgs[1] = (char*)"-c";
            bashArgs[2] = (char*)line.c_str();
            bashArgs[3] = nullptr;

            execv("/bin/bash", bashArgs);
            perror("smash error: execv failed");
            exit(1);
        } else {
            char* args[COMMAND_MAX_ARGS] = {nullptr};
            _parseCommandLine(line.c_str(), args);

            execvp(args[0], args);
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    // parent process
    if (isBackground) {
        SmallShell::getInstance().jobsList.addJob(originalCmd, pid, false);
    } else {
	SmallShell::getInstance().fgPid = pid;
        if (waitpid(pid, nullptr, 0) == -1) {
            perror("smash error: waitpid failed");
            return;
        }
    }
}
void ForegroundCommand::execute() {
    jobs->removeFinishedJobs();

    char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    JobsList::JobEntry* job = nullptr;
    int jobId = -1;

    if (argn == 1) {
        job = jobs->getLastJob(&jobId);

        if (job == nullptr) {
            std::cerr << "smash error: fg: jobs list is empty" << std::endl;
            return;
        }
    }
    else if (argn == 2) {
        for (int i = 0; args[1][i] != '\0'; i++) {
            if (!isdigit(args[1][i])) {
                std::cerr << "smash error: fg: invalid arguments" << std::endl;
                return;
            }
        }

        jobId = atoi(args[1]);
        job = jobs->getJobById(jobId);

        if (job == nullptr) {
            std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
    }
    else {
        std::cerr << "smash error: fg: invalid arguments" << std::endl;
        return;
    }

    pid_t pid = job->pid;
    std::string command = job->cmd_line;

    std::cout << command;
    std::cout << " " << pid << std::endl;

    jobs->removeJobById(jobId);

    if (kill(pid, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }

    SmallShell::getInstance().fgPid = pid;

    if (waitpid(pid, nullptr, 0) == -1) {
        perror("smash error: waitpid failed");
    }

    SmallShell::getInstance().fgPid = -1;
}
void PipeCommand::execute() {
    std::string line = _trim(std::string(cmd_line));

    bool pipeStderr = false;
    size_t pipePos = line.find("|&");

    if (pipePos != std::string::npos) {
        pipeStderr = true;
    } else {
        pipePos = line.find("|");
    }

    if (pipePos == std::string::npos) {
        return;
    }

    std::string cmd1 = _trim(line.substr(0, pipePos));
    std::string cmd2;

    if (pipeStderr) {
        cmd2 = _trim(line.substr(pipePos + 2));
    } else {
        cmd2 = _trim(line.substr(pipePos + 1));
    }

    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    pid_t pid1 = fork();

    if (pid1 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid1 == 0) {
        setpgrp();

        close(pipefd[0]);

        if (pipeStderr) {
            dup2(pipefd[1], STDERR_FILENO);
        } else {
            dup2(pipefd[1], STDOUT_FILENO);
        }

        close(pipefd[1]);

        SmallShell::getInstance().executeCommand(cmd1.c_str());
        exit(0);
    }

    pid_t pid2 = fork();

    if (pid2 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid2 == 0) {
        setpgrp();

        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        SmallShell::getInstance().executeCommand(cmd2.c_str());
        exit(0);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}
static std::string readFileContent(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return "N/A";
    }

    std::string value;
    std::getline(file, value);

    if (value.empty()) {
        return "N/A";
    }

    return value;
}
void USBInfoCommand::execute() {
    std::string basePath = "/sys/bus/usb/devices";

    DIR* dir = opendir(basePath.c_str());

    if (dir == nullptr) {
        perror("smash error: opendir failed");
        return;
    }

    struct UsbDevice {
        int devNum;
        std::string vendor;
        std::string product;
        std::string manufacturer;
        std::string productName;
        std::string maxPower;
    };

    std::vector<UsbDevice> devices;

    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        std::string deviceName = entry->d_name;

        if (deviceName == "." || deviceName == "..") {
            continue;
        }

        std::string devicePath = basePath + "/" + deviceName;

        std::string devNumStr = readFileContent(devicePath + "/devnum");
        std::string vendor = readFileContent(devicePath + "/idVendor");
        std::string product = readFileContent(devicePath + "/idProduct");

        // Skip entries that are not real USB devices
        if (devNumStr == "N/A" || vendor == "N/A" || product == "N/A") {
            continue;
        }

        UsbDevice dev;

        dev.devNum = atoi(devNumStr.c_str());
        dev.vendor = vendor;
        dev.product = product;
        dev.manufacturer = readFileContent(devicePath + "/manufacturer");
        dev.productName = readFileContent(devicePath + "/product");
        dev.maxPower = readFileContent(devicePath + "/bMaxPower");

        devices.push_back(dev);
    }

    closedir(dir);

    if (devices.empty()) {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }

    std::sort(devices.begin(), devices.end(),
              [](const UsbDevice& a, const UsbDevice& b) {
                  return a.devNum < b.devNum;
              });

    for (const UsbDevice& dev : devices) {
        std::cout << "Device "
                  << dev.devNum
                  << ": ID "
                  << dev.vendor
                  << ":"
                  << dev.product
                  << " "
                  << dev.manufacturer
                  << " "
                  << dev.productName
                  << " MaxPower: "
                  << dev.maxPower
                  << std::endl;
    }
}
Command *SmallShell::CreateCommand(const char *cmd_line) {
    SmallShell & smash = SmallShell::getInstance();
    char* args[20]={nullptr};
    int argn = _parseCommandLine(cmd_line,args);
    if (argn == 0) {return nullptr;}
    
    std::string cmd_s = _trim(std::string(cmd_line));
    string firstWord = args[0];

    if (firstWord.compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    } else if (firstWord.compare("unalias") == 0) {
        return new UnAliasCommand(cmd_line);
    }

    if (cmd_s.find(">") != std::string::npos) {
        return new RedirectionCommand(cmd_line);
    }
    if (cmd_s.find("|") != std::string::npos) {
        return new PipeCommand(cmd_line);
    }

    if (firstWord.compare("chprompt")==0){
       if(argn>1){smash.smash=args[1];} else {smash.smash="smash";}
       return nullptr;
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }else if (firstWord.compare("cd") == 0) {
      return new ChangeDirCommand(cmd_line,0);
    }else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, &smash.jobsList);
    }else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, &smash.jobsList);
    }else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line, &smash.jobsList);
    }else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, &smash.jobsList);
    }else if (firstWord.compare("unsetenv") == 0) {
        return new UnSetEnvCommand(cmd_line);
    }else if (firstWord.compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
    }else if (firstWord.compare("du") == 0) {
        return new DiskUsageCommand(cmd_line);
    }else if (firstWord.compare("whoami") == 0) {
        return new WhoAmICommand(cmd_line);
    }else if (firstWord.compare("usbinfo") == 0) {
        return new USBInfoCommand(cmd_line);
    }
    else {
      return new ExternalCommand(cmd_line);
    }
    
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    std::string line = _trim(std::string(cmd_line));

    if (line.empty()) {
        return;
    }

    char* args[20] = {nullptr};
    int argn = _parseCommandLine(line.c_str(), args);

    if (argn == 0) {
        return;
    }

    std::string firstWord = args[0];

    // Do not replace alias command itself
    if (firstWord != "alias" && firstWord != "unalias") {
        for (const auto& alias : aliases) {
            if (alias.first == firstWord) {
                std::string rest = "";

                size_t pos = line.find(firstWord);
                if (pos != std::string::npos) {
                    rest = line.substr(pos + firstWord.length());
                }

                line = alias.second + rest;
                break;
            }
        }
    }

    Command* cmd = CreateCommand(line.c_str());

    if (cmd) {
        cmd->execute();
        delete cmd;
    }
}
void ChangeDirCommand::execute() {
    char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    if (argn < 2) {
        return;
    }

    if (argn > 2) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }

    SmallShell& smash = SmallShell::getInstance();

    char cwd[COMMAND_MAX_LENGTH];

    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }

    if (strcmp(args[1], "-") == 0) {
        if (smash.lastDir.empty()) {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }

        std::string oldDir = smash.lastDir;

        if (chdir(oldDir.c_str()) == -1) {
            perror("smash error: chdir failed");
            return;
        }

        smash.lastDir = cwd;
        return;
    }

    if (chdir(args[1]) == -1) {
        perror("smash error: chdir failed");
        return;
    }

    smash.lastDir = cwd;
}
