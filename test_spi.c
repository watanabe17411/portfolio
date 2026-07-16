// test_spi.c
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

int spi_fd = -1;

void write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_data[2] = { (reg << 1) & 0x7E, value };
    uint8_t rx_data[2] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data, .rx_buf = (unsigned long)rx_data,
        .len = 2, .speed_hz = 4000000, .bits_per_word = 8
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

uint8_t read_reg(uint8_t reg) {
    uint8_t tx_data[2] = { ((reg << 1) & 0x7E) | 0x80, 0x00 };
    uint8_t rx_data[2] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data, .rx_buf = (unsigned long)rx_data,
        .len = 2, .speed_hz = 4000000, .bits_per_word = 8
    };
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) return 0;
    return rx_data[1];
}

int main() {
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    uint8_t mode = SPI_MODE_0;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    printf("=== TEST 1: MFRC522 レジスタ読み書き検証 ===\n");
    
    // 1. バージョン確認
    uint8_t ver = read_reg(0x37);
    printf("1. VersionReg(0x37): 0x%02X (期待値: 0x91 または 0x92)\n", ver);

    // 2. 連続読み書きテスト (タイマーレジスタ 0x2C を使用)
    printf("2. 連続レジスタ整合性テストを開始します...\n");
    uint8_t test_values[3] = {0x55, 0xAA, 0x1E};
    for(int i=0; i<3; i++) {
        write_reg(0x2C, test_values[i]);
        usleep(1000);
        uint8_t read_val = read_reg(0x2C);
        printf("   [Write]: 0x%02X -> [Read]: 0x%02X_... ", test_values[i], read_val);
        if(test_values[i] == read_val) {
            printf("OK\n");
        } else {
            printf("❌ FAIL (ビット化けの可能性)\n");
        }
    }

    close(spi_fd);
    return 0;
}
