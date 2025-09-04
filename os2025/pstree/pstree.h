#define MAX_PIDS 32768
#define MAX_NAME_LEN 256
#define VERSION_INFO "pstree 2025.03 by Terminus"

typedef struct {
	int pid;
	int ppid;
	char name[MAX_NAME_LEN];
} ProcessInfo;

bool getProcessInfo(int pid);
void printUsage();
void printTree(int pid, int level, bool flag);
bool traverseDirectory();
void quickSort(ProcessInfo arr[], int low, int high);
