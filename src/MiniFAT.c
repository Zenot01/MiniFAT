#include <stdio.h>
#include "MiniFAT.h"
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster){
    if (buffer == NULL || size <= 0 || first_cluster == 0xFFF8) return NULL;

    uint8_t *clust = (uint8_t*) buffer;
    size_t size_clust = (double)size/1.5;

    struct clusters_chain_t *chain = calloc(1, sizeof(struct clusters_chain_t));
    if (chain == NULL) return NULL;

    uint16_t cur_clust = first_cluster;


    while (1)
    {
        if (cur_clust > size_clust)
        {
            if (chain->clusters != NULL) free(chain->clusters);
            free(chain);
            return NULL;
        }

        if (chain->clusters == NULL){
            chain->clusters = calloc(1, sizeof(uint16_t));
            if (chain->clusters == NULL){
                free(chain);
                return NULL;
            }
            chain->size++;
            chain->clusters[chain->size - 1] = cur_clust;
        }
        else{
            chain->size++;
            uint16_t *help_clusters = realloc(chain->clusters, chain->size * sizeof(uint16_t));
            if (help_clusters == NULL){
                free(chain->clusters);
                free(chain);
                return NULL;
            }
            chain->clusters = help_clusters;
            chain->clusters[chain->size - 1] = cur_clust;
        }


        size_t offset = cur_clust + (cur_clust / 2);
        uint16_t new_clust = 0;
        uint16_t firstPart = 0;
        uint16_t secPart = 0;
        if (cur_clust % 2 == 0)
        {
            uint8_t a = clust[offset];
            uint8_t b = clust[offset + 1];
            if (a && b) a = 1;
            new_clust = clust[offset] | ((clust[offset + 1] & 0x0F) << 8);
        }
        else
        {
            firstPart = (clust[offset] & 0xF0) >> 4;
            secPart = (clust[offset + 1]) << 4;
            new_clust = firstPart | secPart;
        }
        cur_clust = new_clust;



        if (cur_clust >= 0xFF8 || cur_clust == 0) break;

    }

    return chain;
}




struct disk_t* disk_open_from_file(const char* volume_file_name){
    if (volume_file_name == NULL){
        errno = EFAULT;
        return NULL;
    }

    FILE *f = fopen(volume_file_name, "rb");
    if (f == NULL){
        errno = ENOENT;
        return NULL;
    }

    struct disk_t *disk = calloc(1, sizeof(struct disk_t));
    if (disk == NULL){
        errno = ENOMEM;
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    disk->FileSize = size;
    disk->file = f;

    return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if (pdisk == NULL || buffer == NULL){
        errno = EFAULT;
        return -1;
    }
    if (pdisk->file == NULL){
        errno = EFAULT;
        return -1;
    }


    size_t offset = first_sector * 512;
    size_t bytes_to_read = sectors_to_read * 512;
    if (offset + bytes_to_read > pdisk->FileSize || offset > pdisk->FileSize){
        errno = ERANGE;
        return -1;
    }

    fseek(pdisk->file,(long) offset, SEEK_SET);

    size_t res = fread(buffer, sizeof(uint8_t), bytes_to_read, pdisk->file);
    if (res != bytes_to_read){
        errno = ERANGE;
        return -1;
    }

    return res;
}


int disk_close(struct disk_t* pdisk){
    if (pdisk == NULL){
        errno = EFAULT;
        return -1;
    }
    if (pdisk->file == NULL){
        free(pdisk);
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->file);
    free(pdisk);
    return 0;
}

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if (pdisk == NULL){
        errno = EFAULT;
        return NULL;
    }

    uint8_t Msector[512];
    if (disk_read(pdisk, first_sector, Msector, 1) == -1){
        return NULL;
    }

    if (Msector[510] != 0x55 || Msector[511] != 0xAA){
        errno = EINVAL;
        return NULL;
    }
    uint16_t bytes_per_sector = Msector[11] | (Msector[12] << 8);
    uint8_t sectors_per_cluster = Msector[13];
    uint16_t reserved_sectors = Msector[14] | (Msector[15] << 8);
    uint8_t fat_count = Msector[16];
    uint16_t root_dir_capacity = Msector[17] | (Msector[18] << 8);
    uint16_t logical_sectors16 = Msector[19] | (Msector[20] << 8);
    uint32_t logical_sectors32 = *(uint32_t*)&Msector[32];
    uint16_t sectors_per_fat = Msector[22] | (Msector[23] << 8);


    if (sectors_per_cluster != 1 && sectors_per_cluster != 2 && sectors_per_cluster != 4 && sectors_per_cluster != 8 && sectors_per_cluster != 16 && sectors_per_cluster != 32 && sectors_per_cluster != 64 && sectors_per_cluster != 128){
        errno = EINVAL;
        return NULL;
    }
    if (fat_count != 1 && fat_count != 2){
        errno = EINVAL;
        return NULL;
    }
    if ((root_dir_capacity * 32) % bytes_per_sector != 0){
        errno = EINVAL;
        return NULL;
    }
    if ((logical_sectors16 == 0 && logical_sectors32 == 0)){
        errno = EINVAL;
        return NULL;
    }


    struct volume_t *volume = calloc(sizeof(struct volume_t), 1);
    if (volume == NULL){
        errno = ENOMEM;
        return NULL;
    }
    volume->logical_sectors16 = logical_sectors16;
    volume->logical_sectors32 = logical_sectors32;
    volume->bytes_per_sector = bytes_per_sector;
    volume->root_dir_capacity = root_dir_capacity;
    volume->fat_count = fat_count;
    volume->reserved_sectors = reserved_sectors;
    volume->sectors_per_cluster = sectors_per_cluster;
    volume->sectors_per_fat = sectors_per_fat;
    volume->disk = pdisk;
    volume->root_start = first_sector + reserved_sectors + fat_count * sectors_per_fat;
    volume->sectors_per_root = (root_dir_capacity * 32) / bytes_per_sector;
    volume->volume_start = first_sector;

    return volume;
}



int fat_close(struct volume_t* pvolume){
    if (pvolume == NULL){
        errno = EFAULT;
        return -1;
    }
    if (pvolume->FAT != NULL) free(pvolume->FAT);
    free(pvolume);
    return 0;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if (pvolume == NULL || file_name == NULL){
        errno = EFAULT;
        return NULL;
    }

    uint8_t sector[512];
    for (size_t a = 0; a < pvolume->sectors_per_root; ++a) {
        disk_read(pvolume->disk, pvolume->root_start + a, sector, 1);
        for (int j = 0; j < pvolume->bytes_per_sector / 32; ++j) {
            int ind = j * 32;

            if (sector[ind] == 0xE5 || sector[ind] == 0x00) continue;

            char name[9], type[4], whole_name[13];
            for (int i = 0; i < 13; ++i) whole_name[i] = '\0';
            memcpy(name, sector + ind, 8);
            memcpy(type, sector + ind + 8, 3);
            for (int i = 7; i >= 0; --i) {
                if (name[i] != ' ') break;
                else name[i] = '\0';
            }
            for (int i = 2; i >= 0; --i) {
                if (type[i] != ' ') break;
                else type[i] = '\0';
            }
            int k = 0;
            for (; name[k] != '\0'; ++k) whole_name[k] = name[k];
            if (type[0] != '\0') whole_name[k] = '.';
            k++;
            for (int i = 0; type[i] != '\0'; ++k,++i) whole_name[k] = type[i];
            whole_name[k] = '\0';

            if (strcmp(whole_name, file_name) == 0){
                if (sector[ind + 11] & 0x10 || sector[ind + 11] & 0x08){
                    errno = EISDIR;
                    return NULL;
                }
                struct file_t *file = calloc(1, sizeof(struct file_t));
                if (file == NULL){
                    errno = ENOMEM;
                    return NULL;
                }
                file->pvolume = pvolume;
                file->size = *(uint32_t *)((sector + ind) + 28);
                file->start_cluster = (sector[ind + 21] << 24) | (sector[ind + 20] << 16) | (sector[ind + 27] << 8) | sector[ind + 26];
                if (pvolume->FAT == NULL){
                    uint8_t *FAT = calloc(sizeof(uint8_t), pvolume->sectors_per_fat * pvolume->bytes_per_sector);
                    if (FAT == NULL){
                        errno = EFAULT;
                        return NULL;
                    }
                    disk_read(pvolume->disk, pvolume->volume_start + pvolume->reserved_sectors, FAT, pvolume->sectors_per_fat);
                    pvolume->FAT = FAT;
                }
                file->chain = get_chain_fat12(pvolume->FAT, pvolume->bytes_per_sector * pvolume->sectors_per_fat,file->start_cluster);
                if (file->chain == NULL){
                    errno = EFAULT;
                    return NULL;
                }
                return file;
            }
        }
    }
    errno = ENOENT;
    return NULL;
}

int file_close(struct file_t* stream){
    if (stream == NULL){
        errno = EFAULT;
        return -1;
    }
    if (stream->chain != NULL){
        if (stream->chain->clusters != NULL) free(stream->chain->clusters);
        free(stream->chain);
    }
    free(stream);
    return 0;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if (stream == NULL){
        errno = EFAULT;
        return -1;
    }

    if (whence == SEEK_SET){
        if ((size_t)offset > stream->size || offset < 0){
            errno = ENXIO;
            return -1;
        }
        stream->pos = offset;
    }
    else if (whence == SEEK_END){
        if ((size_t)offset * (-1) > stream->size || offset > 0){
            errno = ENXIO;
            return -1;
        }
        stream->pos = stream->size + offset;
    }
    else if (whence == SEEK_CUR){
        if (offset >= 0){
            if ((size_t)offset + stream->pos > stream->size){
                errno = ENXIO;
                return -1;
            }
        }
        else if (offset < 0){
            if ((size_t)offset * (-1) + stream->pos > stream->size){
                errno = ENXIO;
                return -1;
            }
        }
        stream->pos += offset;
    }
    else{
        errno = EINVAL;
        return -1;
    }

    return stream->pos;
}

size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if (ptr == NULL || stream == NULL){
        errno = EFAULT;
        return -1;
    }

    size_t bytes_read = 0;
    size_t ind = 0;
    size_t bytes_to_read = nmemb * size;
    size_t bytes_viewed = 0;
    size_t nmemb_read = 0;
    uint64_t data_start = stream->pvolume->volume_start + stream->pvolume->reserved_sectors + stream->pvolume->fat_count * stream->pvolume->sectors_per_fat + stream->pvolume->sectors_per_root;


    for (size_t i = 0; i < stream->chain->size + 1; ++i) {
        size_t clust = stream->chain->clusters[i];

        for (int sect = 0; sect < stream->pvolume->sectors_per_cluster; ++sect) {
            uint8_t buffer[512];
            if (disk_read(stream->pvolume->disk, data_start + (clust - 2) * stream->pvolume->sectors_per_cluster + sect, buffer, 1) == -1){
                return -1;
            }

            for (int j = 0; j < 512; ++j)
            {
                if (bytes_viewed == stream->pos){
                    if (bytes_to_read == 0){
                        return nmemb;
                    }
                    if (nmemb_read * size + size > stream->size || bytes_viewed == stream->size){
                        return nmemb_read;
                    }
                    *((uint8_t*)ptr + ind) = buffer[j];
                    ind++;
                    bytes_to_read --;
                    bytes_read++;
                    stream->pos++;
                    if (bytes_read % size == 0) nmemb_read++;
                }
                bytes_viewed++;
            }
        }
    }
    errno = ENXIO;
    return -1;
}

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    if (pvolume == NULL || dir_path == NULL){
        errno = EFAULT;
        return NULL;
    }
    if (strcmp(dir_path, "\\") != 0){
        errno = ENOENT;
        return NULL;
    }

    struct dir_t *dir = calloc(sizeof(struct dir_t), 1);
    if (dir == NULL){
        errno = ENOMEM;
        return NULL;
    }
    uint8_t buffer[512];
    disk_read(pvolume->disk, pvolume->root_start, buffer, 1);
    if ((!(buffer[11] & 0x10)) || (buffer[11] & 0x08)){
        errno = ENOTDIR;
        return NULL;
    }


    dir->volume = pvolume;

    return dir;
}

int dir_close(struct dir_t* pdir){
    if (pdir == NULL){
        errno = EFAULT;
        return -1;
    }
    free(pdir);
    return 0;
}

int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if (pdir == NULL || pentry == NULL){
        errno = EFAULT;
        return -1;
    }
    if (pdir->step == 15 && pdir->num_of_sect == pdir->volume->sectors_per_root){
        return 1;
    }
    if (pdir->step == 16){
        pdir->step = 0;
        pdir->num_of_sect++;
    }


    for (; pdir->num_of_sect < pdir->volume->sectors_per_root; ++pdir->num_of_sect){
        uint8_t buffer[512];
        if (disk_read(pdir->volume->disk, pdir->volume->root_start + pdir->num_of_sect, buffer, 1) == -1){
            errno = EIO;
            return -1;
        }

        for (; pdir->step < 16; ++pdir->step){
            int ind = pdir->step * 32;

            if (buffer[ind] == 0xE5 || buffer[ind] == 0x00) continue;

            for (int i = 0; i < 13; ++i) pentry->name[i] = '\0';

            char name[9], type[4];
            memcpy(name, buffer + ind, 8);
            memcpy(type, buffer + ind + 8, 3);
            for (int i = 7; i >= 0; --i) {
                if (name[i] != ' ') break;
                else name[i] = '\0';
            }
            for (int i = 2; i >= 0; --i) {
                if (type[i] != ' ') break;
                else type[i] = '\0';
            }
            name[8] = '\0';
            type[3] = '\0';
            int k = 0;
            for (; name[k] != '\0'; ++k) pentry->name[k] = name[k];
            if (type[0] != '\0') pentry->name[k] = '.';
            k++;
            for (int i = 0; type[i] != '\0'; ++k,++i) pentry->name[k] = type[i];
            pentry->name[k] = '\0';

            if (buffer[ind + 11] & 0x01) pentry->is_readonly = 1;
            if (buffer[ind + 11] & 0x02) pentry->is_hidden = 1;
            if (buffer[ind + 11] & 0x04) pentry->is_system = 1;
            if (buffer[ind + 11] & 0x10) pentry->is_directory = 1;
            if (buffer[ind + 11] & 0x20) pentry->is_archived = 1;

            pentry->size = *(uint32_t *)(buffer + ind + 28);

            pdir->step++;
            return 0;
        }
        pdir->step = 0;
    }

    return 1;
}