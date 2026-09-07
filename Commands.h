// Ver: 04-11-2025
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <map>
#include <utility>
#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#include <cctype>

using namespace std;
std::string _ltrim(const std::string &s);
std::string _rtrim(const std::string &s);
std::string _trim(const std::string &s);
int _parseCommandLine(const char *cmd_line, char **args);
bool _isBackgroundCommand(const char *cmd_line);
void _removeBackgroundSign(char *cmd_line);
const std::string WHITESPACE = " \n\r\t\f\v";

#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <cstdlib>
#include <cstring>
static bool isReservedKeyword(const std::string& name) {
    return name == "chprompt" ||
           name == "showpid" ||
           name == "pwd" ||
           name == "cd" ||
           name == "jobs" ||
           name == "fg" ||
           name == "quit" ||
           name == "kill" ||
           name == "alias" ||
           name == "unalias";
}
using namespace std;
static bool isNumber(const char* str) {
    if (str == nullptr || str[0] == '\0') {
        return false;
    }

    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }

    return true;
}
static bool isValidAliasName(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    for (char c : name) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }

    return true;
}

class Command {
public:
    const char *cmd_line;

    Command(const char *cmd_line1){cmd_line=cmd_line1;} ;

    virtual ~Command(){};

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line): Command(cmd_line){}

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line): Command(cmd_line){}

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line): Command(cmd_line) {}

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line):Command(cmd_line) {}

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line): Command(cmd_line) {}

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line): Command(cmd_line) {}

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class USBInfoCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    USBInfoCommand(const char *cmd_line): Command(cmd_line) {}

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd): BuiltInCommand(cmd_line) {}

    virtual ~ChangeDirCommand(){
            
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~GetCurrDirCommand(){
    }

    void execute() override{
	char cwd[COMMAND_MAX_LENGTH];

        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            perror("smash error: getcwd failed");
            return;
        }

        std::cout << cwd << std::endl;
    };
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~ShowPidCommand(){
    }

    void execute() override{
	std::cout << "smash pid is " << getpid() << std::endl;
    };
};

class JobsList;


class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t pid;
        std::string cmd_line;
        bool isStopped;

        JobEntry(int job_id, pid_t pid, const std::string& cmd_line, bool isStopped)
            : job_id(job_id), pid(pid), cmd_line(cmd_line), isStopped(isStopped) {}
    };

private:
    std::vector<JobEntry> jobs;
    int maxJobId;

public:
    JobsList(): maxJobId(0) {};

    ~JobsList(){};

    void addJob(const std::string& cmd_line, pid_t pid, bool isStopped) {
	    removeFinishedJobs();

	    int newJobId = 1;

	    for (const JobEntry& job : jobs) {
		if (job.job_id >= newJobId) {
		    newJobId = job.job_id + 1;
		}
	    }

	    jobs.push_back(JobEntry(newJobId, pid, cmd_line, isStopped));

	    if (newJobId > maxJobId) {
		maxJobId = newJobId;
	    }
	};
    void printJobsList(){
	    removeFinishedJobs();

	    std::sort(jobs.begin(), jobs.end(),
		      [](const JobEntry& a, const JobEntry& b) {
		          return a.job_id < b.job_id;
		      });

	    for (const JobEntry& job : jobs) {
		std::cout << "[" << job.job_id << "] " << job.cmd_line << std::endl;
	    }
	};

    void killAllJobs(){
    removeFinishedJobs();

    std::cout << "smash: sending SIGKILL signal to "
              << jobs.size()
              << " jobs:"
              << std::endl;

    std::sort(jobs.begin(), jobs.end(),
              [](const JobEntry& a, const JobEntry& b) {
                  return a.job_id < b.job_id;
              });

    for (const JobEntry& job : jobs) {
        std::cout << job.pid << ": " << job.cmd_line << std::endl;

        if (kill(job.pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
    }

    jobs.clear();
};

    void removeFinishedJobs()	{
	    for (auto it = jobs.begin(); it != jobs.end(); ) {
		int status = 0;
		pid_t result = waitpid(it->pid, &status, WNOHANG);

		if (result == it->pid) {
		    it = jobs.erase(it);
		} else {
		    ++it;
		}
	    }
	}

    JobEntry *getJobById(int jobId){
    for (auto& job : jobs) {
        if (job.job_id == jobId) {
            return &job;
        }
    }
    return nullptr;
};

    void removeJobById(int jobId){
    for (auto it = jobs.begin(); it != jobs.end(); ++it) {
        if (it->job_id == jobId) {
            jobs.erase(it);
            return;
        }
    }
};

    JobEntry *getLastJob(int *lastJobId){
    if (jobs.empty()) {
        return nullptr;
    }

    JobEntry* lastJob = &jobs[0];

    for (auto& job : jobs) {
        if (job.job_id > lastJob->job_id) {
            lastJob = &job;
        }
    }

    if (lastJobId != nullptr) {
        *lastJobId = lastJob->job_id;
    }

    return lastJob;
};

    JobEntry *getLastStoppedJob(int *jobId);
};
class JobsCommand : public BuiltInCommand {
private:
    JobsList* jobs;

public:
    JobsCommand(const char *cmd_line, JobsList *jobs)
        : BuiltInCommand(cmd_line), jobs(jobs) {}

    virtual ~JobsCommand() {
    }

    void execute() override{
    jobs->printJobsList();
};
};

class ForegroundCommand : public BuiltInCommand {
JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs)  {}

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
public:
    JobsList* jobs;


    QuitCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line),jobs(jobs) {}

    virtual ~QuitCommand() {
    }

    void execute() override{char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    if (argn > 1 && strcmp(args[1], "kill") == 0) {
        jobs->killAllJobs();
    }

    exit(0);};
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};
class KillCommand : public BuiltInCommand {
public:
    JobsList* jobs;
    KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line),jobs(jobs) {}

    virtual ~KillCommand() {
    }

    void execute() override{char* args[20] = {nullptr};
    int argn = _parseCommandLine(cmd_line, args);

    // Format must be: kill -<signum> <jobid>
    if (argn != 3) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    // args[1] must start with '-'
    if (args[1][0] != '-') {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    // after '-' must be a number
    if (!isNumber(args[1] + 1)) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    // job id must be a number
    if (!isNumber(args[2])) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    int signum = atoi(args[1] + 1);
    int jobId = atoi(args[2]);

    JobsList::JobEntry* job = jobs->getJobById(jobId);

    if (job == nullptr) {
        std::cerr << "smash error: kill: job-id "
                  << jobId
                  << " does not exist"
                  << std::endl;
        return;
    }

    if (kill(job->pid, signum) == -1) {
        perror("smash error: kill failed");
        return;
    }

    std::cout << "signal number "
              << signum
              << " was sent to pid "
              << job->pid
              << std::endl;};
};

class SmallShell {
private:

    SmallShell();

public:
    std::string smash="smash";
    std::string lastDir="";
    JobsList jobsList;
    std::vector<std::pair<std::string, std::string>> aliases;
    pid_t fgPid = -1;
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
