/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "nvs.h"
#include "esp_flash_partitions.h"

extern const void *bootloader_mmap(uint32_t src_addr, uint32_t size);
extern void bootloader_munmap(const void *mapping);

static int nvs_key_type2str(nvs_type_t type,char* out_value, size_t* length)
{
    switch(type) {
    case NVS_TYPE_U8:
        if(out_value)
            strcpy(out_value,"U8");
        if(length)
            *length = strlen("U8");
        break;
    case NVS_TYPE_I8:
        if(out_value)
            strcpy(out_value,"I8");
        if(length)
            *length = strlen("I8");
        break;
    case NVS_TYPE_U16:
        if(out_value)
            strcpy(out_value,"U16");
        if(length)
            *length = strlen("U16");
        break;
    case NVS_TYPE_I16:
        if(out_value)
            strcpy(out_value,"I16");
        if(length)
            *length = strlen("I16");
        break;
    case NVS_TYPE_U32:
        if(out_value)
            strcpy(out_value,"U32");
        if(length)
            *length = strlen("U32");
        break;
    case NVS_TYPE_U64:
        strcpy(out_value,"U64");
        if(length)
            *length = strlen("U64");
        break;
    case NVS_TYPE_I64:
        if(out_value)
            strcpy(out_value,"I64");
        if(length)
            *length = strlen("I64");
        break;
    case NVS_TYPE_STR:
        strcpy(out_value,"STR");
        if(length)
            *length = strlen("STR");
        break;
    case NVS_TYPE_BLOB:
        if(out_value)
            strcpy(out_value,"BLOB");
        if(length)
            *length = strlen("BLOB");
        break;
    default:
        if(length)
            *length = 0;
        break;
    }
    return 0;
}

static nvs_type_t nvs_key_str2type(char* str)
{
    if(strcmp(str,"U8") == 0)
    {
        return NVS_TYPE_U8;
    }
    else if(strcmp(str,"I8") == 0)
    {
        return NVS_TYPE_I8;
    }else if(strcmp(str,"U16") == 0)
    {
        return NVS_TYPE_U16;
    }else if(strcmp(str,"I16") == 0)
    {
        return NVS_TYPE_I16;
    }else if(strcmp(str,"U32") == 0)
    {
        return NVS_TYPE_U32;
    }else if(strcmp(str,"I32") == 0)
    {
        return NVS_TYPE_I32;
    }else if(strcmp(str,"U64") == 0)
    {
        return NVS_TYPE_U64;
    }else if(strcmp(str,"I64") == 0)
    {
        return NVS_TYPE_I64;
    }else if(strcmp(str,"STR") == 0)
    {
        return NVS_TYPE_STR;
    }else if(strcmp(str,"BLOB") == 0)
    {
        return NVS_TYPE_BLOB;
    }else{
        return NVS_TYPE_ANY;
    }
}
static int nvs_write_vaule(const char *part_name, const char* name, const char* key,nvs_type_t type,const char* value)
{
    nvs_handle handle;
    esp_err_t ret;
    ret = nvs_open_from_partition(part_name, name, NVS_READONLY, &handle);

    if (ret != ESP_OK) {
        printf("nvs open %s failed with %x\r\n", name, ret);
        return -1;
    }
}

static int nvs_printf_vaule(const char *part_name, const char* name, const char* key,nvs_entry_info_t info)
{
    nvs_handle handle;
    esp_err_t ret;
    ret = nvs_open_from_partition(part_name, name, NVS_READONLY, &handle);

    if (ret != ESP_OK) {
        printf("nvs open %s failed with %x\r\n", name, ret);
        return -1;
    }
    switch(info.type) {
    case NVS_TYPE_U8: {
        uint8_t out_value;
        nvs_get_u8(handle,key,&out_value);
        printf("%u\r\n",out_value);
    }
    break;
    case NVS_TYPE_I8: {
        int8_t out_value;
        nvs_get_i8(handle,key,&out_value);
        printf("%d\r\n",out_value);
    }
    break;
    case NVS_TYPE_U16: {
        uint16_t out_value;
        nvs_get_u16(handle,key,&out_value);
        printf("%u\r\n",out_value);
    }
    break;
    case NVS_TYPE_I16: {
        int16_t out_value;
        nvs_get_i16(handle,key,&out_value);
        printf("%d\r\n",out_value);
    }
    break;
    case NVS_TYPE_U32: {
        uint32_t out_value;
        nvs_get_u32(handle,key,&out_value);
        printf("%u\r\n",out_value);
    }
    break;
    case NVS_TYPE_I32: {
        int32_t out_value;
        nvs_get_i32(handle,key,&out_value);
        printf("%d\r\n",out_value);
    }
    break;
    case NVS_TYPE_U64: {
        uint64_t out_value;
        nvs_get_u64(handle,key,&out_value);
        printf("%llu\r\n",out_value);
    }
    break;
    case NVS_TYPE_I64: {
        int64_t out_value;
        nvs_get_i64(handle,key,&out_value);
        printf("%lld\r\n",out_value);
    }
    break;
    case NVS_TYPE_STR: {
        size_t buffer_len = 0;
        nvs_get_str(handle, key, NULL,&buffer_len);
        if(buffer_len) {
            char *buffer = malloc(buffer_len);
            if(buffer) {
                memset(buffer,0,buffer_len);
                nvs_get_str(handle, key, buffer,&buffer_len);
                printf("%s\r\n",buffer);
                free(buffer);
            }
        }
    }
    break;
    case NVS_TYPE_BLOB: {
        size_t buffer_len = 0;
        nvs_get_blob(handle, key, NULL,&buffer_len);
        if(buffer_len) {
            char *buffer = malloc(buffer_len+1);
            if(buffer) {
                memset(buffer,0,buffer_len+1);
                nvs_get_blob(handle, key, buffer,&buffer_len);
                for(size_t i=0; i<buffer_len; i++) {
                    printf("%02x",buffer[i]);
                }
                printf("\r\n");
                printf("%s\r\n",buffer);
                free(buffer);
            }
        }
    }
    break;
    default:
        break;
    }
    nvs_close(handle);
    return 0;
}

static int nvs_print_command(int argc, char **argv)
{
    nvs_iterator_t it = NULL;
    char type[8];
    char namespace_name[16]= {0};
    const esp_partition_info_t *partitions = NULL;
    const char *partition_usage;
    esp_err_t err;
    int num_partitions = 1;
    if(argc == 1) {
        partitions = bootloader_mmap(ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
        if (!partitions) {
            printf("bootloader_mmap(0x%x, 0x%x) failed\r\n", ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
            return -1;
        }
        printf("mapped partition table 0x%x at 0x%x\r\n", ESP_PARTITION_TABLE_OFFSET, (intptr_t)partitions);
        err = esp_partition_table_verify(partitions, true, &num_partitions);
        if (err != ESP_OK) {
            printf("Failed to verify partition table\r\n");
            return -1;
        }
    } else if(argc == 2) {
        it = nvs_entry_find(argv[1], NULL, NVS_TYPE_ANY);
    } else if(argc == 3 || argc == 4) {
        it = nvs_entry_find(argv[1], argv[2], NVS_TYPE_ANY);
    } else {
        printf("nvs_print nvs\r\n");
        printf("nvs_print nvs iotkit-kv\r\n");
        return -1;
    }
    for (int i = 0; i < num_partitions; i++) {
        if(partitions) {
            const esp_partition_info_t *partition = &partitions[i];
            if(partition->subtype == 0x02) {
                /* print partition type info */
                printf("partition %-16s %08x %08x \r\n",partition->label,
                       partition->pos.offset, partition->pos.size);
                it = nvs_entry_find((const char*)partition->label, NULL, NVS_TYPE_ANY);
            }
        }
        while (it != NULL) {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            if(strcmp(namespace_name,info.namespace_name)!=0) {
                strcpy(namespace_name,info.namespace_name);
                printf("namespace '%s' \r\n",namespace_name);
            }
            it = nvs_entry_next(it);
            nvs_key_type2str(info.type,type,NULL);
            if(argc == 4) {
                if(strcmp(info.key,argv[3])==0) {
                    printf("key '%s', type '%s' \r\n", info.key, type);
                    nvs_printf_vaule(argv[1], argv[2],argv[3],info);
                }
            } else {
                printf("key '%s', type '%s' \r\n", info.key, type);
            }
        }
    }
    if(partitions) {
        bootloader_munmap(partitions);
    }
    return 0;
}
static int nvs_write_command(int argc, char **argv)
{
    nvs_handle handle;
    esp_err_t ret;

    if (argc != 6) {
        printf("nvs_write STR nvs iotkit-kv stassid 123456\r\n");
        return -1;
    }
    ret = nvs_open_from_partition(argv[2], argv[3], NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        printf("nvs open %s failed with %x\r\n", argv[3], ret);
        return -1;
    }
    nvs_type_t type = nvs_key_str2type(argv[1]);
    switch(type) {
        case NVS_TYPE_U8: {
            ret = nvs_set_u8(handle, argv[4],(uint8_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_I8: {
            ret = nvs_set_i8(handle, argv[4],(int8_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_U16: {
            ret = nvs_set_u16(handle, argv[4],(uint16_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_I16: {
            ret = nvs_set_i16(handle, argv[4],(int16_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_U32: {
            ret = nvs_set_u32(handle, argv[4],(uint32_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_I32: {
            ret = nvs_set_i32(handle, argv[4],(int32_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_U64: {
            ret = nvs_set_u64(handle, argv[4],(uint64_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_I64: {
            ret = nvs_set_i64(handle, argv[4],(int64_t)atoi(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_STR: {
            ret = nvs_set_str(handle, argv[4],argv[5]);
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        case NVS_TYPE_BLOB: {
            ret = nvs_set_blob(handle, argv[4],argv[5],strlen(argv[5]));
            if (ret != ESP_OK) {
                printf("nvs write %s failed with %x\r\n", argv[4], ret);
            }
        }
        break;
        default:
        break;
    }
    nvs_close(handle);
    return 0;
}
static int nvs_delete_command(int argc, char **argv)
{
    nvs_handle handle;
    esp_err_t ret;

    if (argc != 4) {
        printf("nvs_delete nvs iotkit-kv stassid\r\n");
        return -1;
    }
    ret = nvs_open_from_partition(argv[1], argv[2], NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        printf("nvs open %s failed with %x\r\n", argv[2], ret);
        return -1;
    }
    ret = nvs_erase_key(handle, argv[3]);
    if (ret != ESP_OK) {
        printf("nvs delete %s failed with %x\r\n", argv[3], ret);
    }
    nvs_close(handle);
    return 0;
}
static esp_err_t esp_console_register_nvs_print_command(void)
{
    esp_console_cmd_t command = {
        .command = "nvs_print",
        .help = "Print all nvs env",
        .func = &nvs_print_command
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&command));
    return 0;
}
static esp_err_t esp_console_register_nvs_delete_command(void)
{
    esp_console_cmd_t command = {
        .command = "nvs_delete",
        .help = "Delete nvs env",
        .func = &nvs_delete_command
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&command));
    return 0;
}
static esp_err_t esp_console_register_nvs_write_command(void)
{
    esp_console_cmd_t command = {
        .command = "nvs_write",
        .help = "Write nvs env",
        .func = &nvs_write_command
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&command));
    return 0;
}
esp_err_t esp_console_register_nvs_command(void)
{
    esp_console_register_nvs_print_command();
    esp_console_register_nvs_delete_command();
    esp_console_register_nvs_write_command();
    return 0;
}
