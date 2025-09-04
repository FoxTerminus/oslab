#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include "fat32.h"

typedef struct {
    double r, g, b;
} RGB;

void *map_disk(const char *fname);
void recover_file(struct fat32hdr *hdr);
void process_short_entry(union fat32dent *dent, char *filename, u32 *size, u32 *cluster_id);
void process_long_entry(union fat32dent *dent, u64 cluster_addr, u32 offset, char *filename, u32 *size, u32 *cluster_id);
unsigned char ChkSum(unsigned char *pFcbName);
RGB compute_average_color(const char *file_data, size_t size);
double color_distance(RGB c1, RGB c2);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
        exit(1);
    }

    setbuf(stdout, NULL);

    assert(sizeof(struct fat32hdr) == 512); // defensive

    // map disk image to memory
    struct fat32hdr *hdr = map_disk(argv[1]);

    // TODO: frecov
    recover_file(hdr);

    // file system traversal
    munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}

void *map_disk(const char *fname) {
    int fd = open(fname, O_RDWR);

    if (fd < 0) {
        perror(fname);
        goto release;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        perror(fname);
        goto release;
    }

    struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (hdr == (void *)-1) {
        goto release;
    }

    close(fd);

    if (hdr->Signature_word != 0xaa55 ||
            hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size) {
        fprintf(stderr, "%s: Not a FAT file image\n", fname);
        goto release;
    }
    return hdr;

release:
    if (fd > 0) {
        close(fd);
    }
    exit(1);
}

RGB compute_average_color(const char *file_data, size_t size) {
    RGB mean = {0, 0, 0};
    int count = 0;
    
    for (size_t i = 0; i < size; i += 3) {
        if (i + 2 >= size) break;
        mean.b += file_data[i];
        mean.g += file_data[i+1];
        mean.r += file_data[i+2];
        count++;
    }
    
    if (count > 0) {
        mean.r /= count;
        mean.g /= count;
        mean.b /= count;
    }
    return mean;
}

double color_distance(RGB c1, RGB c2) {
    double dr = c1.r - c2.r;
    double dg = c1.g - c2.g;
    double db = c1.b - c2.b;
    return sqrt(dr * dr + dg * dg + db * db);
}

void recover_file(struct fat32hdr *hdr) {
    u32 data_start_sector = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
    u32 cluster_size = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec;
    u64 data_start = (u64)hdr + data_start_sector * hdr->BPB_BytsPerSec;
    u32 total_data_sectors = hdr->BPB_TotSec32 - data_start_sector;
    u32 total_data_clusters = total_data_sectors / hdr->BPB_SecPerClus;

    for (u32 cluster = 2; cluster < 2 + total_data_clusters; cluster++) {
        u64 cluster_addr = data_start + (cluster - 2) * cluster_size;
        for (u32 offset = 0; offset < cluster_size; offset += 32) {
            union fat32dent *dent = (union fat32dent *)(cluster_addr + offset);
            if (dent->short_entry.DIR_Name[0] == 0x00) {
                // End of directory entries
                break;
            }
            if (dent->short_entry.DIR_NTRes != 0) {
                // Not a valid entry
                continue;
            }
            if (dent->short_entry.DIR_Name[0] == 0xE5) {
                // Free entry
                continue;
            }
            if (dent->short_entry.DIR_Name[8] != 'B' || 
                dent->short_entry.DIR_Name[9] != 'M' || 
                dent->short_entry.DIR_Name[10] != 'P') {
                // Not a BMP file
                continue;
            }
            if (dent->short_entry.DIR_Attr == ATTR_ARCHIVE) {
                // Process entry name
                char *filename = malloc(MAX_NAME_LEN * sizeof(char));
                u32 size, cluster_id;
                process_long_entry(dent, cluster_addr, offset, filename, &size, &cluster_id);
                // printf("Found file: %s, Size: %u bytes, Cluster ID: %u\n", filename, size, cluster_id);

                // 原搜索策略
                char *file_data = (char *)(data_start + (cluster_id - 2) * cluster_size);

                // // 启发式搜索策略
                // u32 n_clusters = (size + cluster_size - 1) / cluster_size;
                // char *file_data = malloc(size);
                // if (!file_data) {
                //     fprintf(stderr, "Memory allocation failed for file data\n");
                //     free(filename);
                //     continue;
                // }

                // // 读取第一个簇
                // u64 curr_addr = data_start + (cluster_id - 2) * cluster_size;
                // size_t to_copy = (size < cluster_size) ? size : cluster_size;
                // memcpy(file_data, (void*)curr_addr, to_copy);
                // size_t copied = to_copy; 

                // // 启发式搜索后续簇
                // u32 current_cluster = cluster_id;
                // for (u32 i = 1; i < n_clusters; i++) {
                //     u32 next_cluster = current_cluster + 1; // 尝试连续簇
                    
                //     // 检查候选簇是否有效
                //     if (next_cluster < 2 || next_cluster >= 2 + total_data_clusters) {
                //         next_cluster = 0; // 无效簇
                //     }
                    
                //     // 计算当前簇尾部RGB均值
                //     char *tail_start = file_data + copied - cluster_size;
                //     size_t tail_len = (copied < cluster_size) ? copied : cluster_size;
                //     RGB tail_mean = compute_average_color(tail_start, tail_len);
                    
                //     // 搜索最佳后续簇
                //     u32 best_cluster = 0;
                //     double min_dist = 1e9;
                    
                //     // 扫描所有簇
                //     u32 scan_start = (current_cluster > 50) ? current_cluster - 50 : 2;
                //     u32 scan_end = (current_cluster + 50 < 2 + total_data_clusters) ? current_cluster + 50 : 2 + total_data_clusters - 1;
                    
                //     for (u32 cand = scan_start; cand <= scan_end; cand++) {
                //         if (cand == current_cluster) continue;
                        
                //         u64 cand_addr = data_start + (cand - 2) * cluster_size;
                //         RGB head_mean = compute_average_color((char *)cand_addr, cluster_size);
                //         double dist = color_distance(tail_mean, head_mean);
                        
                //         if (dist < min_dist) {
                //             min_dist = dist;
                //             best_cluster = cand;
                //         }
                //     }
                    
                //     // 确定最终要使用的簇
                //     u32 use_cluster = (min_dist < 10.0) ? best_cluster : next_cluster;
                //     if (!use_cluster) use_cluster = best_cluster;

                //     // 读取簇数据
                //     u64 use_addr = data_start + (use_cluster - 2) * cluster_size;
                //     to_copy = (size - copied < cluster_size) ? size - copied : cluster_size;
                //     memcpy(file_data + copied, (void*)use_addr, to_copy);
                //     copied += to_copy;
                //     current_cluster = use_cluster;
                // }

                char temp_filename[] = "/tmp/fsrecov_XXXXXX";
                int fd_temp = mkstemp(temp_filename);
                if (fd_temp < 0) {
                    continue;
                }

                ssize_t written = write(fd_temp, file_data, size);
                close(fd_temp);
                if (written != size) {
                    unlink(temp_filename);
                    continue;
                }

                char command[256];
                sprintf(command, "sha1sum '%s'", temp_filename);
                FILE *sha1_pipe = popen(command, "r");
                if (sha1_pipe) {
                    char sha1_output[50];
                    if (fgets(sha1_output, sizeof(sha1_output), sha1_pipe)) {
                        sha1_output[40] = '\0';
                        printf("%s  %s\n", sha1_output, filename);
                    }
                    pclose(sha1_pipe);
                }
                unlink(temp_filename);
                continue;
            }
        }
    }
}

void process_short_entry(union fat32dent *dent, char *filename, u32 *size, u32 *cluster_id) {
    char filenames[13], base[9], ext[4];
    memcpy(base, dent->short_entry.DIR_Name, 8);
    base[8] = '\0';
    memcpy(ext, dent->short_entry.DIR_Name + 8, 3);
    ext[3] = '\0';
    // Remove trailing spaces from base
    for (int i = 7; i >= 0 && base[i] == ' '; i--) {
        base[i] = '\0';
    }
    // Remove trailing spaces from ext
    for (int i = 2; i >= 0 && ext[i] == ' '; i--) {
        ext[i] = '\0';
    }
    if (ext[0] == '\0') {
        snprintf(filenames, sizeof(filenames), "%s", base);
    } else {
        snprintf(filenames, sizeof(filenames), "%s.%s", base, ext);
    }

    // Set the output parameters
    if (filename) {
        strncpy(filename, filenames, strlen(filenames) + 1);
        filename[strlen(filenames)] = '\0'; // Ensure null termination
    }
    if (size) {
        *size = dent->short_entry.DIR_FileSize;
    }
    if (cluster_id) {
        *cluster_id = dent->short_entry.DIR_FstClusLO + (dent->short_entry.DIR_FstClusHI << 16);
    }

    // Print the filename and size

    // printf("Found file: %s, Size: %u bytes, Cluster ID: %u\n", filename, *size, *cluster_id);
}

void process_long_entry(union fat32dent *dent, u64 cluster_addr, u32 offset, char *filename, u32 *size, u32 *cluster_id) {
    unsigned char expected_chksum = ChkSum(dent->short_entry.DIR_Name);
    char filenames[MAX_NAME_LEN] = {0};
    u32 name_len = 0;
    // u32 entry_count = 0;

    // printf("Processing long entry at offset %u\n", offset);

    // 从当前短目录项向前回溯（每次32字节）
    for (u32 ofs = offset; ofs >= 32; ofs -= 32) {
        union fat32dent *long_dent = (union fat32dent *)(cluster_addr + ofs - 32);
        
        // 检查是否为有效长目录项
        if (long_dent->long_entry.LDIR_Attr != ATTR_LONG_NAME) {
            // printf("Attribute mismatch: expected ATTR_LONG_NAME, got %02x\n", long_dent->long_entry.LDIR_Attr);
            break;  // 遇到非长目录项，终止回溯
        }
        
        // 验证校验和
        if (long_dent->long_entry.LDIR_Chksum != expected_chksum) {
            // printf("Checksum mismatch: expected=%02x, actual=%02x\n", expected_chksum, long_dent->long_entry.LDIR_Chksum);
            break;
        }

        // 检查结束标记（最高位0x40）
        // if (long_dent->long_entry.LDIR_Ord & 0x40) 
        //     entry_count = long_dent->long_entry.LDIR_Ord & 0x1F;

        for (int i = 0; i < 5; i++) {
            if (long_dent->long_entry.LDIR_Name1[i] == 0 || long_dent->long_entry.LDIR_Name1[i] == 0xFFFF) {
                break;  // 遇到空字符，停止读取
            }
            if (name_len < MAX_NAME_LEN - 1) {
                filenames[name_len++] = long_dent->long_entry.LDIR_Name1[i];
            } else {
                fprintf(stderr, "Filename too long, truncating\n");
                break;
            }
        }
        for (int i = 0; i < 6; i++) {
            if (long_dent->long_entry.LDIR_Name2[i] == 0 || long_dent->long_entry.LDIR_Name2[i] == 0xFFFF) {
                break;  // 遇到空字符，停止读取
            }
            if (name_len < MAX_NAME_LEN - 1) {
                filenames[name_len++] = long_dent->long_entry.LDIR_Name2[i];
            } else {
                fprintf(stderr, "Filename too long, truncating\n");
                break;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (long_dent->long_entry.LDIR_Name3[i] == 0 || long_dent->long_entry.LDIR_Name3[i] == 0xFFFF) {
                break;  // 遇到空字符，停止读取
            }
            if (name_len < MAX_NAME_LEN - 1) {
                filenames[name_len++] = long_dent->long_entry.LDIR_Name3[i];
            } else {
                fprintf(stderr, "Filename too long, truncating\n");
                break;
            }
        }
        // 检查是否是最后一个长目录项
        if (long_dent->long_entry.LDIR_Ord & 0x40) {
            // 最后一个长目录项，结束处理
            filenames[name_len] = '\0';  // 确保字符串以null结尾
            break;
        }
    }

    if (filenames[0] != '\0') {
        // printf("Found file: %s, Size: %u bytes, Cluster ID: %u\n", filenames, dent->short_entry.DIR_FileSize, dent->short_entry.DIR_FstClusLO + (dent->short_entry.DIR_FstClusHI << 16));
        // printf("Total long entries: %d\n", entry_count);
    }
    else {
        process_short_entry(dent, filename, size, cluster_id);
    }

    // Set the output parameters
    if (filename) {
        strncpy(filename, filenames, strlen(filenames) + 1);
        filename[strlen(filenames)] = '\0'; // Ensure null termination
    }
    if (size) {
        *size = dent->short_entry.DIR_FileSize;
    }
    if (cluster_id) {
        *cluster_id = dent->short_entry.DIR_FstClusLO + (dent->short_entry.DIR_FstClusHI << 16);
    }
}

unsigned char ChkSum (unsigned char *pFcbName) { 
    short FcbNameLen; 
    unsigned char Sum; 
    Sum = 0; 
    for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) { 
        // NOTE: The operation is an unsigned char rotate right 
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++; 
    }
    return (Sum); 
}
