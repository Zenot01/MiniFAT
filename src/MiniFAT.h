#ifndef MINIFAT_H
#define MINIFAT_H

#include "stdint.h"


struct disk_t{
    FILE *file;
    size_t FileSize;
};
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t{
    struct disk_t *disk;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint16_t sectors_per_fat;
    uint32_t logical_sectors32;
    int root_start;
    uint64_t sectors_per_root;
    uint64_t volume_start;
    uint8_t *FAT;
};
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct file_t{
    struct volume_t *pvolume;
    uint32_t pos;
    size_t size;
    uint32_t start_cluster;
    struct clusters_chain_t *chain;
};
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t{
    struct volume_t *volume;
    size_t step;
    size_t num_of_sect;
};
struct dir_entry_t {
    char name[13];
    uint32_t size;
    uint8_t is_archived;
    uint8_t is_readonly;
    uint8_t is_system;
    uint8_t is_hidden;
    uint8_t is_directory;
};
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);




struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};
struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster);

#endif //MINIFAT_H
