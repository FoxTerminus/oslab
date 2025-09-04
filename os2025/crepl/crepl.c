#include <testkit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>

typedef struct {
    char path[256];
    int fd;
} TempFile;

TempFile decls_file;         
TempFile func_files[100];    
TempFile expr_files[100];
int func_count = 0;
int expr_count = 0;
void *func_handles[100];
int func_handle_count = 0;

int create_tempfile(TempFile *tf, const char *suffix) {
    char base_template[] = "/tmp/crepl-XXXXXX";    
    int fd = mkstemp(base_template);
    if (fd == -1) {
        perror("mkstemp failed");
        return -1;
    }

    snprintf(tf->path, sizeof(tf->path), "%s%s", base_template, suffix);
    if (rename(base_template, tf->path) == -1) {
        perror("rename failed");
        close(fd);
        return -1;
    }

    tf->fd = fd;
    return 0;
}

// void cleanup() {
//     printf("\nCleaning temporary files...\n");
    
//     close(decls_file.fd);
//     remove(decls_file.path);

//    for (int i = 0; i < func_count; i++) {
//         close(func_files[i].fd);
//         remove(func_files[i].path);
//         char so_path[256];
//         snprintf(so_path, sizeof(so_path), "%s.so", func_files[i].path);
//         remove(so_path);
//     }

//     for (int i = 0; i < expr_count; i++) {
//         close(expr_files[i].fd);
//         remove(expr_files[i].path);
//         char so_path[256];
//         snprintf(so_path, sizeof(so_path), "%s.so", expr_files[i].path);
//         remove(so_path);
//     }
// }

void setup() {
    if (create_tempfile(&decls_file, ".h") != 0) {
        exit(EXIT_FAILURE);
    }
    
    const char *init_header = "// Function declarations\n";
    write(decls_file.fd, init_header, strlen(init_header));
    
    // atexit(cleanup);
}

void add_declaration(const char *decl) {
    char full_decl[512];
    snprintf(full_decl, sizeof(full_decl), "%s;\n", decl);
    write(decls_file.fd, full_decl, strlen(full_decl));
    fsync(decls_file.fd);
}

bool compile_shared_lib(const TempFile *src, const char *suffix) {
    char output_path[256];
    snprintf(output_path, sizeof(output_path), "%s.so", src->path);

    pid_t pid = fork();
    if (pid == 0) {
        int dev_null = open("/dev/null", O_WRONLY);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);

        execlp("gcc", "gcc", "-fPIC", "-shared", 
               "-o", output_path, src->path, 
               "-include", decls_file.path, 
               NULL);
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

bool compile_and_load_function(const char *line) {
    setup();
    regex_t regex;
    regmatch_t matches[3];
    char pattern[] = "^int[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*\\(([^)]*)\\)[[:space:]]*\\{.*";
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return 0;
    }
    if (regexec(&regex, line, 3, matches, 0) != 0) {
        regfree(&regex);
        return 0;
    }
    int func_name_len = matches[1].rm_eo - matches[1].rm_so;
    int args_len = matches[2].rm_eo - matches[2].rm_so;
    char func_name[256] = {0};
    char args[256] = {0};
    strncpy(func_name, line + matches[1].rm_so, func_name_len);
    strncpy(args, line + matches[2].rm_so, args_len);
    regfree(&regex);
    if (func_count >= 100) {
        printf("Too many functions\n");
        return 0;
    }
    TempFile *tf = &func_files[func_count++];
    if (create_tempfile(tf, ".c") != 0) return 0;

    dprintf(tf->fd, "#include <%s>\n\n%s\n", decls_file.path, line);
    fsync(tf->fd);

    if (!compile_shared_lib(tf, ".so")) {
        printf("Compile error\n");
        return 0;
    }

    char so_path[256];
    snprintf(so_path, sizeof(so_path), "%s.so", tf->path);
    void *handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        printf("Load error: %s\n", dlerror());
        return 0;
    }

    if (func_handle_count < 100) {
        func_handles[func_handle_count++] = handle;
    } else {
        dlclose(handle);
        return 0;
    }
    printf("OK.\n");
    return 1;
}

bool evaluate_expression(const char *expr, int* result) {
    setup();
    if (expr_count >= 100) {
        printf("Too many expressions\n");
        return 0;
    }
    TempFile *tf = &expr_files[expr_count++];
    if (create_tempfile(tf, "_expr.c") != 0) return 0;

    dprintf(tf->fd, "#include <%s>\n"
                   "int __expr_wrapper() { "
                   "return (%s); }\n", 
                   decls_file.path, expr);
    fsync(tf->fd);

    if (!compile_shared_lib(tf, ".so")) {
        printf("Compile error\n");
        return 0;
    }

    char so_path[256];
    snprintf(so_path, sizeof(so_path), "%s.so", tf->path);
    void *handle = dlopen(so_path, RTLD_NOW);
    if (!handle) {
        printf("Load error: %s\n", dlerror());
        return 0;
    }

    int (*func)() = dlsym(handle, "__expr_wrapper");
    if (!func) {
        printf("Symbol error: %s\n", dlerror());
        dlclose(handle);
        return 0;
    }

    *result = func();
    printf("= %d.\n", func());
    dlclose(handle);
    return 1;
}

int main() {
    // setup();
    // return 0;
    char *line = NULL;
    size_t len = 0;
    printf(">> ");
    fflush(stdout);
    int result;
    while (getline(&line, &len, stdin) != -1) {
        size_t read = strlen(line);
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        if (strncmp(line, "int ", 4) == 0) {
            if (!compile_and_load_function(line)) {
                printf("Error.\n");
            }
        } else {
            if (!evaluate_expression(line, &result)) {
                printf("Error.\n");
            }
        }
        printf(">> ");
        fflush(stdout);
        // free(result);
    }
    // free(result);
    free(line);
    for (int i = 0; i < func_handle_count; i++) {
        dlclose(func_handles[i]);
    }
    return 0;
}
