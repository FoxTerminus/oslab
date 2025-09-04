#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "pstree.h"
#include <testkit.h>

ProcessInfo processes[MAX_PIDS];
int process_count = 0;

int main(int argc, char *argv[]) {
	int opt;
	bool pid_flag = false;
	bool sort_flag = false;
	bool version_flag = false;

	static struct option long_options[] = {
		{"show-pids", no_argument, 0, 'p'},
		{"numeric-sort", no_argument, 0, 'n'},
		{"version", no_argument, 0, 'V'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "pnV", long_options, 0))) {
		if (opt == '?') {
			printUsage();
			return 1;
		}
		if (opt == -1){
			break;
		}

		switch (opt) {
			case 'p':
				pid_flag = true;
				break;
			case 'n':
				sort_flag = true;
				break;
			case 'V':
				version_flag = true;
				break;
		}
	}

	if (optind < argc) {
		printUsage();
		return 1;
	}

	if (version_flag) {
		if (!pid_flag && !sort_flag) {
			printf(VERSION_INFO);
			printf("\n");
			return 0;
		}
		return 1;
	}

	if (!traverseDirectory()) {
		return 1;
	}

	if (pid_flag) {
		printf("systemd(1)\n");
	}
	else {
		printf("systemd\n");
	}

	printTree(1, 1, pid_flag);
	return 0;
}

void printUsage() {
	printf("Usage:\n");
	printf("  pstree --show-pids\n");
	printf("  pstree --numeric-sort\n");
	printf("  pstree --show-pids --numeric-sort\n");
	printf("  pstree --version\n");
	printf("  pstree -p\n");
	printf("  pstree -n\n");
	printf("  pstree -p -n\n");
	printf("  pstree -V\n");
}

bool getProcessInfo(int pid) {
	char path[64];
	char line[256];
	FILE* status_file = NULL;

	snprintf(path, sizeof(path), "/proc/%d/status", pid);
	status_file = fopen(path, "r");

	if (!status_file) {
		perror("Failed to open status file");
		return false;
	}

	processes[process_count].pid = pid;
	processes[process_count].ppid = -1;
	processes[process_count].name[0] = '\0';

	while (fgets(line, sizeof(line), status_file)) {
		if (strncmp(line, "PPid:", 5) == 0) {
			sscanf(line+5, "%d", &processes[process_count].ppid);
		}
		else if (strncmp(line, "Name:", 5) == 0) {
			sscanf(line+6, "%s", processes[process_count].name);
		}
	}

	fclose(status_file);
	process_count++;
	return true;
}

bool traverseDirectory() {
	DIR* proc_dir = NULL;
	struct dirent* entry = NULL;

	proc_dir = opendir("/proc");
	if (!proc_dir) {
		perror("Failed to open /proc");
		return false;
	}

	while ((entry = readdir(proc_dir)) != NULL) {
		if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
			int pid = atoi(entry->d_name);
			if (!getProcessInfo(pid)) {
				return false;
			}
		}
	}

	closedir(proc_dir);
	return true;
}

void printTree(int pid, int level, bool flag) {
	srand(time(NULL));
	quickSort(processes, 0, process_count-1);
	for (int i = 0; i < process_count; i++) {
		if (processes[i].ppid == pid) {
			for (int j = 0; j < level; j++) {
				printf("  ");
			}
			if (flag) {
				printf("%s(%d)\n", processes[i].name, processes[i].pid);
			}
			else {
				printf("%s\n", processes[i].name);
			}
			printTree(processes[i].pid, level+1, flag);
		}
	}
}

void swap(ProcessInfo *a, ProcessInfo *b) {
    ProcessInfo temp = *a;
    *a = *b;
    *b = temp;
}

int randomPartition(ProcessInfo arr[], int low, int high) {
    int random = low + rand() % (high - low + 1);
    swap(&arr[random], &arr[high]);

    ProcessInfo pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j].pid <= pivot.pid) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }

    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

void quickSort(ProcessInfo arr[], int low, int high) {
    if (low < high) {
        int pi = randomPartition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}
