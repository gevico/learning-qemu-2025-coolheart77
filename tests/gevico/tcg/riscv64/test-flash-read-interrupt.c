/*
 * Test SPI CS functionality for G233 platform
 *
 * Copyright (c) 2025 hongquan.li hongquan.prog@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

 #include "crt.h"

/* Log level definitions */
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR  1
#define LOG_LEVEL_WRN  2
#define LOG_LEVEL_INF  3
#define LOG_LEVEL_DBG  4

/* Current log level for this file */
#define CURRENT_LOG_LEVEL LOG_LEVEL_INF

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_WHITE   "\033[37m"

/* Log macros with color and level control */
#define LOG_ERR(fmt, ...) do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERR) { \
        printf(COLOR_RED "[ERR] " fmt COLOR_RESET "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_WRN(fmt, ...) do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_WRN) { \
        printf(COLOR_YELLOW "[WRN] " fmt COLOR_RESET "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_INF(fmt, ...) do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INF) { \
        printf(COLOR_GREEN "[INF] " fmt COLOR_RESET "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_DBG(fmt, ...) do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DBG) { \
        printf(COLOR_WHITE "[DBG] " fmt COLOR_RESET "\n", ##__VA_ARGS__); \
    } \
} while(0)

/* G233 SPI register definitions */
#define G233_SPI_BASE_ADDR 0x10018000

/* SPI Control Register 1 (SPI_CR1) */
#define SPI_CR1_OFFSET      0x00
#define SPI_CR1_CPHA        (1 << 0)   /* Clock Phase */
#define SPI_CR1_CPOL        (1 << 1)   /* Clock Polarity */
#define SPI_CR1_MSTR        (1 << 2)   /* Master Selection */
#define SPI_CR1_BR_SHIFT    3          /* Baud Rate Control shift */
#define SPI_CR1_SPE         (1 << 6)   /* SPI Enable */
#define SPI_CR1_LSBFIRST    (1 << 7)   /* Frame Format */
#define SPI_CR1_SSI         (1 << 8)   /* Internal Slave Select */
#define SPI_CR1_SSM         (1 << 9)   /* Software Slave Management */
#define SPI_CR1_RXONLY      (1 << 10)  /* Receive Only */
#define SPI_CR1_DFF         (1 << 11)  /* Data Frame Format */
#define SPI_CR1_CRCNEXT     (1 << 12)  /* CRC Transfer Next */
#define SPI_CR1_CRCEN       (1 << 13)  /* Hardware CRC Calculation Enable */
#define SPI_CR1_BIDIOE      (1 << 14)  /* Output Enable in Bidirectional Mode */
#define SPI_CR1_BIDIMODE    (1 << 15)  /* Bidirectional Data Mode Enable */

/* SPI Control Register 2 (SPI_CR2) */
#define SPI_CR2_OFFSET      0x04
#define SPI_CR2_TXEIE        (1 << 7)   /* TXE interrupt enable */
#define SPI_CR2_RXNEIE       (1 << 6)   /* RXNE interrupt enable */
#define SPI_CR2_ERRIE        (1 << 5)   /* Error interrupt enable */
#define SPI_CR2_SSOE         (1 << 4)   /* Software slave select output enable */

/* SPI Status Register (SPI_SR) */
#define SPI_SR_OFFSET       0x08
#define SPI_SR_RXNE         (1 << 0)   /* Receive buffer Not Empty */
#define SPI_SR_TXE          (1 << 1)   /* Transmit buffer Empty */
#define SPI_SR_CHSIDE       (1 << 2)   /* Channel side */
#define SPI_SR_UDR          (1 << 3)   /* Underrun flag */
#define SPI_SR_CRCERR       (1 << 4)   /* CRC error flag */
#define SPI_SR_MODF         (1 << 5)   /* Mode fault */
#define SPI_SR_OVR          (1 << 6)   /* Overrun flag */
#define SPI_SR_BSY          (1 << 7)   /* Busy flag */

/* SPI Data Register (SPI_DR) */
#define SPI_DR_OFFSET       0x0C

/* SPI CS Control Register (SPI_CSCTRL) */
#define SPI_CSCTRL_OFFSET   0x10
#define SPI_CSCTRL_CS0_EN   (1 << 0)   /* CS0 Enable */
#define SPI_CSCTRL_CS1_EN   (1 << 1)   /* CS1 Enable */
#define SPI_CSCTRL_CS2_EN   (1 << 2)   /* CS2 Enable */
#define SPI_CSCTRL_CS3_EN   (1 << 3)   /* CS3 Enable */
#define SPI_CSCTRL_CS0_ACT  (1 << 4)   /* CS0 Active */
#define SPI_CSCTRL_CS1_ACT  (1 << 5)   /* CS1 Active */
#define SPI_CSCTRL_CS2_ACT  (1 << 6)   /* CS2 Active */
#define SPI_CSCTRL_CS3_ACT  (1 << 7)   /* CS3 Active */

/* Memory-mapped register access */
#define REG32(addr) (*(volatile uint32_t *)(G233_SPI_BASE_ADDR + (addr)))

/* W25X16 Flash Commands */
#define W25X16_READ_DATA       0x03  /* Read Data */
#define W25X16_READ_STATUS     0x05  /* Read Status Register */
#define W25X16_WRITE_ENABLE    0x06  /* Write Enable */
#define W25X16_PAGE_PROGRAM    0x02  /* Page Program */
#define W25X16_SECTOR_ERASE    0x20  /* Sector Erase (4KB) */
#define W25X16_READ_JEDEC_ID   0x9F  /* Read JEDEC ID */

/* Interrupt-driven SPI transfer state */
typedef struct {
    volatile uint8_t *tx_buffer;
    volatile uint8_t *rx_buffer;
    volatile int tx_index;
    volatile int rx_index;
    volatile int total_bytes;
    volatile int transfer_complete;
    volatile int error_occurred;
    volatile int interrupt_count;
} spi_transfer_state_t;

static spi_transfer_state_t spi_state;

/* SPI Interrupt Handler */
void spi0_interrupt_handler(void)
{
    uint32_t sr = REG32(SPI_SR_OFFSET);
    uint32_t cr2 = REG32(SPI_CR2_OFFSET);
    
    spi_state.interrupt_count++;
    
    printf("IRQ #%d: SR=0x%02X, CR2=0x%02X\n", 
        spi_state.interrupt_count, sr, cr2);

    /* Check for error conditions first */
    if ((cr2 & SPI_CR2_ERRIE) && (sr & (SPI_SR_UDR | SPI_SR_OVR))) {
        LOG_ERR("IRQ #%d: UDR=%d, OVR=%d", 
            spi_state.interrupt_count, 
            !!(sr & SPI_SR_UDR), !!(sr & SPI_SR_OVR));
        spi_state.error_occurred = 1;
        return;
    }
    
    /* Handle RXNE interrupt - receive data */
    if ((cr2 & SPI_CR2_RXNEIE) && (sr & SPI_SR_RXNE)) {
        uint8_t received_byte = REG32(SPI_DR_OFFSET) & 0xFF;
        LOG_DBG("IRQ #%d RXNE: Received raw byte (0x%02X)", 
            spi_state.interrupt_count, received_byte);
        
        /* Store all received bytes - don't skip any */
        if (spi_state.rx_index < spi_state.total_bytes) {
            spi_state.rx_buffer[spi_state.rx_index] = received_byte;
            LOG_DBG("IRQ #%d RXNE: Stored byte %d (0x%02X)", 
                spi_state.interrupt_count, 
                spi_state.rx_index, 
                spi_state.rx_buffer[spi_state.rx_index]);
            spi_state.rx_index++;
        }
    }
    
    /* Handle TXE interrupt - send next byte */
    if ((cr2 & SPI_CR2_TXEIE) && (sr & SPI_SR_TXE)) {
        if (spi_state.tx_index < spi_state.total_bytes) {
            /* Send next byte */
            REG32(SPI_DR_OFFSET) = spi_state.tx_buffer[spi_state.tx_index];
            LOG_DBG("IRQ #%d TXE: Sent byte %d (0x%02X)", 
                spi_state.interrupt_count, 
                spi_state.tx_index, 
                spi_state.tx_buffer[spi_state.tx_index]);
            spi_state.tx_index++;
        } else {
            /* All bytes sent, check if we need to continue receiving */
            if (spi_state.rx_index < spi_state.total_bytes) {
                /* Still need to receive remaining bytes, send dummy data */
                REG32(SPI_DR_OFFSET) = 0x00;
                LOG_DBG("IRQ #%d TXE: All bytes sent, sending dummy for RX (rx_index=%d/%d)", 
                    spi_state.interrupt_count, spi_state.rx_index, spi_state.total_bytes);
            } else {
                /* All done - both TX and RX complete */
                spi_state.transfer_complete = 1;
                REG32(SPI_CR2_OFFSET) = cr2 & ~(SPI_CR2_TXEIE | SPI_CR2_RXNEIE);
                LOG_DBG("IRQ #%d TXE: Transfer complete, interrupts disabled", 
                    spi_state.interrupt_count);
            }
        }
    }
}

static void g233_spi_init(void)
{
    /* Reset SPI */
    REG32(SPI_CR1_OFFSET) = 0;
    REG32(SPI_CR2_OFFSET) = 0;
    
    /* Configure SPI as master, 8-bit data, MSB first */
    uint32_t cr1 = SPI_CR1_MSTR |           /* Master mode */
                   (0x3 << SPI_CR1_BR_SHIFT) | /* Baud rate: fPCLK/16 */
                   SPI_CR1_SSM |            /* Software slave management */
                   SPI_CR1_SSI |            /* Internal slave select */
                   SPI_CR1_SPE;             /* Enable SPI */
    
    REG32(SPI_CR1_OFFSET) = cr1;
    
    LOG_INF("G233 SPI initialized with interrupt support");
}

static void g233_spi_cs_assert(void)
{
    /* Enable and activate CS0 */
    REG32(SPI_CSCTRL_OFFSET) = SPI_CSCTRL_CS0_EN | SPI_CSCTRL_CS0_ACT;
}

static void g233_spi_cs_deassert(void)
{
    /* Disable CS0 */
    REG32(SPI_CSCTRL_OFFSET) = 0;
}

/* Interrupt-driven SPI transfer function */
static int g233_spi_transfer_interrupt(const uint8_t *tx_data, uint8_t *rx_data, int len)
{
    int timeout_count = 0;
    const int max_timeout = 100000; /* Maximum timeout count */
    
    /* Initialize transfer state */
    spi_state.tx_buffer = (volatile uint8_t *)tx_data;
    spi_state.rx_buffer = rx_data;
    spi_state.tx_index = 0;
    spi_state.rx_index = 0;
    spi_state.total_bytes = len;
    spi_state.transfer_complete = 0;
    spi_state.error_occurred = 0;
    spi_state.interrupt_count = 0;
    
    LOG_INF("Starting interrupt-driven SPI transfer (%d bytes)", len);
    
    /* Enable interrupts for both TXE and RXNE */
    REG32(SPI_CR2_OFFSET) = SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE;
    
    /* Wait for transfer completion */
    printf("Starting transfer...\n");
    while (!spi_state.transfer_complete && !spi_state.error_occurred && timeout_count < max_timeout) {
        /* Don't write to DR here - let interrupts handle the transfer */
        for (volatile int i = 0; i < 100; i++); /* Small delay */
        timeout_count++;
    }
    printf("Timeout count: %d\n", timeout_count);
    
    /* Disable interrupts */
    REG32(SPI_CR2_OFFSET) = 0;
    
    if (spi_state.error_occurred) {
        LOG_ERR("Transfer failed due to error");
        return -1;
    }
    
    if (timeout_count >= max_timeout) {
        LOG_WRN("Transfer timeout after %d iterations", max_timeout);
        return -2;
    }
    
    LOG_INF("Transfer completed successfully in %d interrupts", spi_state.interrupt_count);
    return 0;
}

static uint8_t flash_read_status(void)
{
    uint8_t cmd = W25X16_READ_STATUS;
    uint8_t status = 0xFF; /* Default to busy state if transfer fails */
    uint8_t tx_data[2] = {cmd, 0x00};
    uint8_t rx_data[2] = {0, 0};
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    if (g233_spi_transfer_interrupt(tx_data, rx_data, 2) == 0) {
        status = rx_data[1];
        LOG_DBG("Flash status read: 0x%02X", status);
    } else {
        LOG_ERR("Failed to read flash status");
    }
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    return status;
}

static uint32_t flash_read_jedec_id(void)
{
    uint8_t cmd = W25X16_READ_JEDEC_ID;
    uint8_t tx_data[4] = {cmd, 0x00, 0x00, 0x00}; /* Command + 3 dummy bytes */
    uint8_t rx_data[4] = {0, 0, 0, 0};
    uint32_t jedec_id = 0;
    
    LOG_INF("Reading JEDEC ID...");
    LOG_DBG("TX data: %02X %02X %02X %02X", tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    if (g233_spi_transfer_interrupt(tx_data, rx_data, 4) == 0) {
        LOG_INF("RX data: %02X %02X %02X %02X", rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
        /* JEDEC ID is returned in bytes 1, 2, 3 (byte 0 is dummy from command byte) */
        /* Manufacturer ID, Device ID high, Device ID low */
        jedec_id = (rx_data[1] << 16) | (rx_data[2] << 8) | rx_data[3];
        LOG_INF("JEDEC ID: 0x%06X (Manufacturer: 0x%02X, Device: 0x%04X)", 
                jedec_id, rx_data[1], (rx_data[2] << 8) | rx_data[3]);
    } else {
        LOG_ERR("Failed to read JEDEC ID");
    }
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    return jedec_id;
}

static void flash_write_enable(void)
{
    uint8_t cmd = W25X16_WRITE_ENABLE;
    uint8_t tx_data[1] = {cmd};
    uint8_t rx_data[1] = {0};
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    g233_spi_transfer_interrupt(tx_data, rx_data, 1);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
}

static void flash_wait_busy(void)
{
    uint8_t status;
    int retry_count = 0;
    
    LOG_DBG("Waiting for flash to become ready...");
    
    do {
        status = flash_read_status();
        retry_count++;
        
        if (retry_count % 1000 == 0) {
            LOG_DBG("Flash busy wait: retry %d, status 0x%02X", retry_count, status);
        }
        
        if (retry_count > 10000) {
            LOG_WRN("Flash busy timeout after %d retries, status: 0x%02X", retry_count, status);
            break;
        }
        for (volatile int i = 0; i < 1000; i++); /* Small delay */
    } while (status & 0x01); /* Check BUSY bit */
    
    LOG_DBG("Flash ready after %d retries, final status: 0x%02X", retry_count, status);
}

static void flash_sector_erase(uint32_t addr)
{
    uint8_t addr_bytes[3];
    uint8_t tx_data[4];
    uint8_t rx_data[4] = {0};
    
    LOG_INF("Erasing sector at address 0x%06X...", addr);
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    /* Prepare command and address */
    tx_data[0] = W25X16_SECTOR_ERASE;
    tx_data[1] = addr_bytes[0];
    tx_data[2] = addr_bytes[1];
    tx_data[3] = addr_bytes[2];
    
    /* Enable write */
    flash_write_enable();
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    g233_spi_transfer_interrupt(tx_data, rx_data, 4);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    /* Wait for erase to complete */
    flash_wait_busy();
    LOG_INF("Sector erase completed");
}

static void flash_page_program(uint32_t addr, const uint8_t *data, int len)
{
    uint8_t addr_bytes[3];
    static uint8_t tx_data[260]; /* Max page size + command + address */
    static uint8_t rx_data[260];
    
    LOG_INF("Programming page at address 0x%06X, length %d bytes...", addr, len);
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    /* Prepare command, address, and data */
    tx_data[0] = W25X16_PAGE_PROGRAM;
    tx_data[1] = addr_bytes[0];
    tx_data[2] = addr_bytes[1];
    tx_data[3] = addr_bytes[2];
    for (int i = 0; i < len; i++) {
        tx_data[4 + i] = data[i];
    }
    
    /* Enable write */
    flash_write_enable();
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    g233_spi_transfer_interrupt(tx_data, rx_data, 4 + len);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    /* Wait for program to complete */
    flash_wait_busy();
    LOG_INF("Page program completed");
}

static void flash_read_data(uint32_t addr, uint8_t *data, int len)
{
    uint8_t addr_bytes[3];
    static uint8_t tx_data[260]; /* Max page size + command + address */
    static uint8_t rx_data[260];
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    /* Prepare command and address */
    tx_data[0] = W25X16_READ_DATA;
    tx_data[1] = addr_bytes[0];
    tx_data[2] = addr_bytes[1];
    tx_data[3] = addr_bytes[2];
    for (int i = 0; i < len; i++) {
        tx_data[4 + i] = 0x00; /* Dummy bytes for reading */
    }
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Use interrupt-driven transfer */
    /* Need to transmit 4 + len + 1 bytes because first RX byte is usually invalid */
    if (g233_spi_transfer_interrupt(tx_data, rx_data, 4 + len) == 0) {
        /* Copy received data (skip command and address bytes) */
        /* The first 4 bytes are command + address, but first RX byte is usually invalid */
        /* So actual data starts at index 5 (4 + 1) */
        LOG_DBG("RX data (first 16 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rx_data[0], rx_data[1], rx_data[2], rx_data[3], rx_data[4], rx_data[5], rx_data[6], rx_data[7],
            rx_data[8], rx_data[9], rx_data[10], rx_data[11], rx_data[12], rx_data[13], rx_data[14], rx_data[15]);
        
        for (int i = 0; i < len; i++) {
            data[i] = rx_data[4 + i]; /* Start from index 5, not 4 */
        }
        
        LOG_DBG("Copied data (first 16 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
        
        LOG_DBG("Copied data (last 16 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            data[len-16], data[len-15], data[len-14], data[len-13], data[len-12], data[len-11], data[len-10], data[len-9],
            data[len-8], data[len-7], data[len-6], data[len-5], data[len-4], data[len-3], data[len-2], data[len-1]);
    }
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
}

static void flash_write_test_data(void)
{
    uint8_t test_data[256] = {0}; /* One page of data */
    uint8_t read_data[256] = {0};
    int errors = 0;
    
    LOG_INF("=== Flash Write/Read Test (Interrupt-driven) ===");
    
    /* Generate test data: ASCII letters A-Z cycling */
    for (int i = 0; i < 256; i++) {
        test_data[i] = 'a' + (i % 26);
    }
    
    LOG_INF("Generated test data (first 32 bytes):");
    for (int i = 0; i < 32; i++) {
        printf("%c", test_data[i]);
    }
    printf("\n");
    
    /* Read status register */
    uint8_t status = flash_read_status();
    LOG_INF("Flash status before operations: 0x%02X", status);
    
    /* Erase first sector (4KB) */
    flash_sector_erase(0x000000);
    
    /* Program first page (256 bytes) */
    flash_page_program(0x000000, test_data, 256);
    
    /* Read back the data */
    LOG_INF("Reading back data from flash...");
    flash_read_data(0x000000, read_data, 256);
    
    /* Show first 32 bytes of read data for debugging */
    LOG_INF("Read data (first 32 bytes):");
    for (int i = 0; i < 32; i++) {
        printf("%c", read_data[i]);
    }
    printf("\n");
    
    /* Compare data */
    LOG_INF("Comparing written vs read data...");
    for (int i = 0; i < 256; i++) {
        if (test_data[i] != read_data[i]) {
            errors++;
            LOG_ERR("Error at offset %d: expected 0x%02X ('%c'), got 0x%02X ('%c')\n",
                i, test_data[i], test_data[i], read_data[i], read_data[i]);
            break;
        }
    }
    
    if (errors == 0) {
        LOG_INF("✓ SUCCESS: All 256 bytes match perfectly!");
    } else {
        LOG_ERR("✗ FAILED: %d bytes don't match", errors);
        crt_assert(0);
    }
    
    /* Show first 32 bytes of read data */
    LOG_INF("Read data (first 32 bytes):");
    for (int i = 0; i < 32; i++) {
        printf("%c", read_data[i]);
    }
    printf("\n");
    
    /* Show hex dump of first 16 bytes */
    LOG_INF("Hex dump (first 16 bytes):");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", read_data[i]);
    }
    printf("\n");
}

int main(void)
{
    LOG_INF("G233 SPI Flash Write/Read Test (Interrupt-driven)");
    LOG_INF("==================================================");
    
    g233_spi_init();
    
    /* Read JEDEC ID after SPI initialization */
    uint32_t jedec_id = flash_read_jedec_id();
    if (jedec_id != 0) {
        LOG_INF("✓ JEDEC ID read successfully: 0x%06X", jedec_id);
    } else {
        LOG_ERR("✗ Failed to read JEDEC ID");
    }
    
    flash_write_test_data();
    
    LOG_INF("Flash write/read test completed!");
    return 0;
}
