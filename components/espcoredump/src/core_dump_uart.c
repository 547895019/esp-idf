// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "soc/uart_periph.h"
#include "soc/gpio_periph.h"
#include "driver/gpio.h"
#include "hal/gpio_hal.h"
#include "esp_core_dump_types.h"
#include "esp_core_dump_port.h"
#include "esp_core_dump_common.h"
#include "esp_partition.h"
#include "esp_flash_internal.h"
#include "esp_flash_encrypt.h"
#include "esp_rom_sys.h"
#include <unistd.h>
#include <fcntl.h>

const static DRAM_ATTR char TAG[] __attribute__((unused)) = "esp_core_dump_uart";

#if CONFIG_ESP_COREDUMP_ENABLE_TO_UART

#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
static DRAM_ATTR uint32_t wr_flash_off = 0;

#ifndef CONFIG_SPI_FLASH_USE_LEGACY_IMPL
#define ESP_COREDUMP_FLASH_WRITE(_off_, _data_, _len_)           spi_flash_write(_off_, _data_, _len_)
#define ESP_COREDUMP_FLASH_WRITE_ENCRYPTED(_off_, _data_, _len_) spi_flash_write_encrypted(_off_, _data_, _len_)
#define ESP_COREDUMP_FLASH_ERASE(_off_, _len_)                   spi_flash_erase_range(_off_, _len_)
#define ESP_COREDUMP_FLASH_READ(_off_, _data_,_len_)             spi_flash_read(_off_, _data_, _len_)
#else
#define ESP_COREDUMP_FLASH_WRITE(_off_, _data_, _len_)           esp_flash_write(esp_flash_default_chip, _data_, _off_, _len_)
#define ESP_COREDUMP_FLASH_WRITE_ENCRYPTED(_off_, _data_, _len_) esp_flash_write_encrypted(esp_flash_default_chip, _off_, _data_, _len_)
#define ESP_COREDUMP_FLASH_ERASE(_off_, _len_)                   esp_flash_erase_region(esp_flash_default_chip, _off_, _len_)
#define ESP_COREDUMP_FLASH_READ(_off_, _data_,_len_)             esp_flash_write(esp_flash_default_chip, _data_, _off_, _len_)
#endif
#endif


/* This function exists on every board, thus, we don't need to specify
 * explicitly the header for each board. */
int esp_clk_cpu_freq(void);

#if CONFIG_ESP_COREDUMP_UART_TO_FILE
static int backtrace_fd = -1;
static int corefd = -1;

static void save_corefile(uint8_t *data, int len)
{
    if(corefd != -1){
        write(corefd, data, len);
        write(corefd,"\r\n", strlen("\r\n"));
    }
}

static char * core_file_path()
{
    int fd;
    static char path[32];

    for (int i = 0; i < 10; i++)
    {
        snprintf(path, sizeof(path), CONFIG_ESP_COREDUMP_UART_CORE_FILE_NAME".%d", i);
        fd = open(path, O_RDONLY);
        if (fd >= 0)
            close(fd);
        else
            return path;
    }

    return NULL;
}

void esp_backtrace_user_print_entry_start(void)
{
    backtrace_fd = open(CONFIG_ESP_COREDUMP_UART_BACKTRACE_FILE_NAME".log", O_RDWR | O_CREAT | O_TRUNC);
}

void esp_backtrace_user_print_entry(uint32_t pc, uint32_t sp)
{
    if(backtrace_fd != -1){
        char  log[32];
        snprintf(log,sizeof(log),DRAM_STR("0x%08X:0x%08X "), pc,sp);
        int ret = write(backtrace_fd, log, strlen(log));
        if(ret == strlen(log))
        {
            fsync(backtrace_fd);
        }
    }
}

void esp_backtrace_user_print_entry_end(void)
{
    if(backtrace_fd != -1){
        close(backtrace_fd);
        backtrace_fd = -1;
    }
}
#endif

static void esp_core_dump_b64_encode(const uint8_t *src, uint32_t src_len, uint8_t *dst) {
    const static DRAM_ATTR char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j, a, b, c;

    for (i = j = 0; i < src_len; i += 3) {
        a = src[i];
        b = i + 1 >= src_len ? 0 : src[i + 1];
        c = i + 2 >= src_len ? 0 : src[i + 2];

        dst[j++] = b64[a >> 2];
        dst[j++] = b64[((a & 3) << 4) | (b >> 4)];
        if (i + 1 < src_len) {
            dst[j++] = b64[(b & 0x0F) << 2 | (c >> 6)];
        }
        if (i + 2 < src_len) {
            dst[j++] = b64[c & 0x3F];
        }
    }
    while (j % 4 != 0) {
        dst[j++] = '=';
    }
    dst[j++] = '\0';
}

#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
static esp_err_t esp_core_dump_partition_and_size_get(const esp_partition_t **partition,uint32_t* size)
{
    uint32_t core_size = 0;
    const esp_partition_t *core_part = NULL;

    /* Check the arguments, at least one should be provided. */
    if (partition == NULL && size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Find the partition that could potentially contain a (previous) core dump. */
    core_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
                                         NULL);
    if (core_part == NULL) {
        ESP_COREDUMP_LOGE("No core dump partition found!");
        return ESP_ERR_NOT_FOUND;
    }
    if (core_part->size < sizeof(uint32_t)) {
        ESP_COREDUMP_LOGE("Too small core dump partition!");
        return ESP_ERR_INVALID_SIZE;
    }

    /* The partition has been found, get its first uint32_t value, which
     * describes the core dump file size. */
    esp_err_t err = esp_partition_read(core_part, 0, &core_size, sizeof(uint32_t));
    if (err != ESP_OK) {
        ESP_COREDUMP_LOGE("Failed to read core dump data size (%d)!", err);
        return err;
    }

    /* Verify that the size read from the flash is not corrupted. */
    if (core_size == 0xFFFFFFFF) {
        ESP_COREDUMP_LOGD("Blank core dump partition!");
        return ESP_ERR_INVALID_SIZE;
    }

    if ((core_size < sizeof(uint32_t)) || (core_size > core_part->size)) {
        ESP_COREDUMP_LOGE("Incorrect size of core dump image: %d", core_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Return the values if needed. */
    if (partition != NULL) {
        *partition = core_part;
    }

    if (size != NULL) {
        *size = core_size;
    }

    return ESP_OK;
}

static esp_err_t esp_core_dump_partition_get(const esp_partition_t **partition)
{
    const esp_partition_t *core_part = NULL;

    /* Check the arguments, at least one should be provided. */
    if (partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Find the partition that could potentially contain a (previous) core dump. */
    core_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
                                         NULL);
    if (core_part == NULL) {
        ESP_COREDUMP_LOGE("No core dump partition found!");
        return ESP_ERR_NOT_FOUND;
    }

    /* Return the values if needed. */
    if (partition != NULL) {
        *partition = core_part;
    }


    return ESP_OK;
}
static esp_err_t esp_core_dump_flash_write_data(esp_partition_t *partition, uint8_t* data, uint32_t data_size)
{
    if(partition)
    {
        ESP_COREDUMP_FLASH_WRITE(partition->address + wr_flash_off, data,data_size);
        wr_flash_off += data_size;
        //+\r\n
        ESP_COREDUMP_FLASH_WRITE(partition->address + wr_flash_off, "\r\n", strlen("\r\n"));
        wr_flash_off += strlen("\r\n");
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t esp_core_dump_flash_write_data_end(esp_partition_t *partition, uint8_t* data, uint32_t data_size)
{
    if(partition)
    {
        ESP_COREDUMP_FLASH_WRITE(partition->address + wr_flash_off, data, data_size);
        wr_flash_off += data_size;
        //+\r\n
        ESP_COREDUMP_FLASH_WRITE(partition->address + wr_flash_off, "\r\n", strlen("\r\n"));
        wr_flash_off += strlen("\r\n");
        //+size
        uint32_t size = wr_flash_off - sizeof(uint32_t);
        ESP_COREDUMP_FLASH_WRITE(partition->address + 0, &size, sizeof(uint32_t));
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
#endif

static esp_err_t esp_core_dump_uart_write_start(core_dump_write_data_t *priv)
{
    esp_err_t err = ESP_OK;
    core_dump_write_data_t *wr_data = (core_dump_write_data_t *)priv;

    ESP_COREDUMP_ASSERT(priv != NULL);
    esp_core_dump_checksum_init(&wr_data->checksum_ctx);
    esp_rom_printf(DRAM_STR("================= CORE DUMP START =================\r\n"));
    return err;
}

static esp_err_t esp_core_dump_uart_write_prepare(core_dump_write_data_t *priv, uint32_t *data_len)
{
    ESP_COREDUMP_ASSERT(data_len != NULL);
    *data_len += esp_core_dump_checksum_size();
#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
    const esp_partition_t *partition = 0;
    uint32_t size = *data_len * 2;
    uint32_t sec_num = 0;
    wr_flash_off = sizeof(uint32_t);
    if (esp_core_dump_partition_get(&partition) == ESP_OK) {
        sec_num = size / SPI_FLASH_SEC_SIZE;
        if (size % SPI_FLASH_SEC_SIZE) {
            sec_num++;
        }
        ESP_COREDUMP_LOGI("Erase flash %d bytes @ 0x%x", sec_num * SPI_FLASH_SEC_SIZE, partition->address + 0);
        ESP_COREDUMP_FLASH_ERASE(partition->address,sec_num * SPI_FLASH_SEC_SIZE);
    }
#endif
#if CONFIG_ESP_COREDUMP_UART_TO_FILE
    char * path = core_file_path();
    if (path)
    {
        corefd = open(path, O_RDWR | O_CREAT | O_TRUNC);
    }
#endif
    return ESP_OK;
}

static esp_err_t esp_core_dump_uart_write_end(core_dump_write_data_t *priv)
{
    esp_err_t err = ESP_OK;
    char buf[64 + 4] = { 0 };
    core_dump_checksum_bytes cs_addr = NULL;
    core_dump_write_data_t *wr_data = (core_dump_write_data_t *)priv;
    
    #if CONFIG_ESP_COREDUMP_UART_TO_FLASH
    const esp_partition_t *partition = 0;
    esp_core_dump_partition_get(&partition);
    #endif
    if (wr_data) {
        size_t cs_len = esp_core_dump_checksum_finish(wr_data->checksum_ctx, &cs_addr);
        wr_data->off += cs_len;
        esp_core_dump_b64_encode((const uint8_t *)cs_addr, cs_len, (uint8_t*)&buf[0]);
        esp_rom_printf(DRAM_STR("%s\r\n"), buf);
#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
        esp_core_dump_flash_write_data_end(partition,(uint8_t*)buf, strlen(buf));
#endif
#if CONFIG_ESP_COREDUMP_UART_TO_FILE
        save_corefile((uint8_t*)buf, strlen(buf));
#endif
    }
    esp_rom_printf(DRAM_STR("================= CORE DUMP END =================\r\n"));

    if (cs_addr) {
        esp_core_dump_print_checksum(DRAM_STR("Coredump checksum"), cs_addr);
    }
#if CONFIG_ESP_COREDUMP_UART_TO_FILE
    if(corefd != -1){
        close(corefd);
        corefd = -1;
    }
#endif
    return err;
}

static esp_err_t esp_core_dump_uart_write_data(core_dump_write_data_t *priv, void * data, uint32_t data_len)
{
    esp_err_t err = ESP_OK;
    char buf[64 + 4] = { 0 };
    char *addr = data;
    char *end = addr + data_len;
    core_dump_write_data_t *wr_data = (core_dump_write_data_t *)priv;

    ESP_COREDUMP_ASSERT(data != NULL);
#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
    const esp_partition_t *partition = 0;
    esp_core_dump_partition_get(&partition);
#endif
    while (addr < end) {
        size_t len = end - addr;
        if (len > 48) len = 48;
        /* Copy to stack to avoid alignment restrictions. */
        char *tmp = buf + (sizeof(buf) - len);
        memcpy(tmp, addr, len);
        esp_core_dump_b64_encode((const uint8_t *)tmp, len, (uint8_t *)buf);
        addr += len;
        esp_rom_printf(DRAM_STR("%s\r\n"), buf);
#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
        esp_core_dump_flash_write_data(partition,(uint8_t*)buf, strlen(buf));
#endif
#if CONFIG_ESP_COREDUMP_UART_TO_FILE
        save_corefile((uint8_t*)buf, strlen(buf));
#endif
    }

    if (wr_data) {
        wr_data->off += data_len;
        esp_core_dump_checksum_update(wr_data->checksum_ctx, data, data_len);
    }
    return err;
}

static int esp_core_dump_uart_get_char(void) {
    int i = -1;
    uint32_t reg = (READ_PERI_REG(UART_STATUS_REG(0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;
    if (reg) {
        i = READ_PERI_REG(UART_FIFO_REG(0));
    }
    return i;
}

void esp_core_dump_to_uart(panic_info_t *info)
{
    core_dump_write_data_t wr_data = { 0 };
    core_dump_write_config_t wr_cfg = {
        .prepare = esp_core_dump_uart_write_prepare,
        .start   = esp_core_dump_uart_write_start,
        .end     = esp_core_dump_uart_write_end,
        .write   = esp_core_dump_uart_write_data,
        .priv    = (void*)&wr_data
    };
    uint32_t tm_end = 0;
    uint32_t tm_cur = 0;
    int ch = 0;

    // TODO: move chip dependent code to portable part
    //Make sure txd/rxd are enabled
    // use direct reg access instead of gpio_pullup_dis which can cause exception when flash cache is disabled
    REG_CLR_BIT(GPIO_PIN_REG_1, FUN_PU);
    gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_U0RXD);
    gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_U0TXD);

    ESP_COREDUMP_LOGI("Press Enter to print core dump to UART...");
    const int cpu_ticks_per_ms = esp_clk_cpu_freq() / 1000;
    tm_end = esp_cpu_get_ccount() / cpu_ticks_per_ms + CONFIG_ESP_COREDUMP_UART_DELAY;
    ch = esp_core_dump_uart_get_char();
    while (!(ch == '\n' || ch == '\r')) {
        tm_cur = esp_cpu_get_ccount() / cpu_ticks_per_ms;
        if (tm_cur >= tm_end){
            break;
        }
        ch = esp_core_dump_uart_get_char();
    }
    ESP_COREDUMP_LOGI("Print core dump to uart...");
    esp_core_dump_write(info, &wr_cfg);
    ESP_COREDUMP_LOGI("Core dump has been written to uart.");
}

void esp_core_dump_init(void)
{
    ESP_COREDUMP_LOGI("Init core dump to UART");
#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
    const esp_partition_t *partition = 0;
    uint32_t size = 0;
    if (esp_core_dump_partition_and_size_get(&partition, &size) == ESP_OK) {
        ESP_COREDUMP_LOGI("Found core dump %d bytes in flash @ 0x%x", size, partition->address);
    }
#endif
}


#if CONFIG_ESP_COREDUMP_UART_TO_FLASH
void esp_core_dump_to_printf(void)
{
    const esp_partition_t *partition = 0;
    uint32_t size = 0;
    esp_rom_printf(DRAM_STR("================= CORE DUMP START =================\r\n"));
    if(esp_core_dump_partition_and_size_get(&partition, &size) == ESP_OK)
    {
        char buf[64 + 4] = { 0 };
        uint32_t addr = sizeof(uint32_t);
        uint32_t end = size + sizeof(uint32_t);
        while (addr < end) {
            size_t len = end - addr;
            if (len > 48) len = 48;
            /* Copy to stack to avoid alignment restrictions. */
            ESP_COREDUMP_FLASH_READ(partition->address + addr, buf, len);
            buf[len] = 0;
            addr += len;
            esp_rom_printf(DRAM_STR("%s"), buf);
        }
    }
    esp_rom_printf(DRAM_STR("================= CORE DUMP END =================\r\n"));
}
#endif
#endif
