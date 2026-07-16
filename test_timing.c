#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define COMMAND_REG     0x01
#define BIT_FRAMING_REG 0x0D
#define FIFO_DATA_REG   0x09
#define FIFO_LEVEL_REG  0x0A

int spi_fd = -1;

void write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_data[2] = { (reg << 1) & 0x7E, value };
    uint8_t rx_data[2] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = 2,
        .speed_hz = 100000,
        .bits_per_word = 8
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

uint8_t read_reg(uint8_t reg) {
    uint8_t tx_data[2] = { ((reg << 1) & 0x7E) | 0x80, 0x00 };
    uint8_t rx_data[2] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = 2,
        .speed_hz = 100000,
        .bits_per_word = 8
    };
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) return 0;
    return rx_data[1];
}

int main() {
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) {
        perror("【エラー】/dev/spidev0.0 オープン失敗");
        return 1;
    }

    uint8_t mode = SPI_MODE_0;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    printf("=== TEST 3: REQAタイミング・ステータス可視化試験 ===\n");
    printf("※カードをRC522に近づけたり離したりしてください（Ctrl+Cで終了）\n\n");

    // 初期化セクション（0x24を完全に排除）
    write_reg(0x01, 0x0F); // SoftReset
    usleep(50000);
    
    write_reg(0x2A, 0x8D); // TModeReg
    write_reg(0x2B, 0x3E); // TPrescalerReg
    write_reg(0x2C, 0x00); // TReloadRegH
    write_reg(0x2D, 0x1E); // TReloadRegL (約25msウィンドウ)
    write_reg(0x15, 0x40); // TxASKReg: 100%ASK
    write_reg(0x11, 0x3D); // ModeReg: CRC初期値 0x6363
    write_reg(0x26, 0x7F); // RxGain: 最大(48dB)
    
    // アンテナ強制ON
    uint8_t tx_ctrl = read_reg(0x14);
    write_reg(0x14, tx_ctrl | 0x03);

    uint32_t loop_count = 0;

    while (1) {
        loop_count++;
        
        // --- ステップ1: カードの存在確認 (REQA: 厳密に7ビット送信仕様) ---
        write_reg(COMMAND_REG, 0x00);        // Idle状態へ
        write_reg(FIFO_LEVEL_REG, 0x80);     // FIFOクリア
        write_reg(0x04, 0x7F);               // ComIrqRegの全フラグをクリア
        
        write_reg(BIT_FRAMING_REG, 0x07);    // REQAは7ビット仕様
        write_reg(FIFO_DATA_REG, 0x26);      // REQAコマンド(0x26)
        write_reg(COMMAND_REG, 0x0C);        // Transceiveコマンド実行
        write_reg(BIT_FRAMING_REG, 0x07 | 0x80); // 送信開始

        usleep(200); // 物理デッドタイム（RF波形送信の猶予）

        uint8_t irq = 0;
        int wait_count = 2000;
        do {
            irq = read_reg(0x04);
            usleep(10);
            wait_count--;
        } while (!(irq & 0x20) && !(irq & 0x01) && (wait_count > 0));

        if ((irq & 0x01) || !(irq & 0x20)) {
            usleep(50000); 
            continue;
        }

        uint8_t n = read_reg(FIFO_LEVEL_REG);
        uint8_t err = read_reg(0x06);

        if (n == 2 && err == 0x00) { 
            // --- ステップ2: 衝突防止 (Anticollision: 端数なし・完全パケット) ---
            write_reg(COMMAND_REG, 0x00);    
            write_reg(FIFO_LEVEL_REG, 0x80); // FIFOクリア
            write_reg(0x04, 0x7F);           // フラグクリア
            
            write_reg(BIT_FRAMING_REG, 0x00); // 完全なバイト通信にリセット
            write_reg(FIFO_DATA_REG, 0x93);   // PICC_ANTICOLL
            write_reg(FIFO_DATA_REG, 0x20);   // NVB
            write_reg(COMMAND_REG, 0x0C);     // Transceive実行
            write_reg(BIT_FRAMING_REG, 0x00 | 0x80); // 送信開始

            usleep(200); 

            wait_count = 2000;
            do {
                irq = read_reg(0x04);
                usleep(10);
                wait_count--;
            } while (!(irq & 0x20) && !(irq & 0x01) && (wait_count > 0));

            err = read_reg(0x06);
            n = read_reg(FIFO_LEVEL_REG);

            printf("[%05d] REQA成功 -> Anticollision検知! FIFO_Lv:%d | ErrorReg:0x%02X\n", 
                   loop_count, n, err);
            
            if (n > 0) {
                printf("   -> UID Raw Data: ");
                for(int i = 0; i < n; i++) {
                    printf("%02X ", read_reg(FIFO_DATA_REG));
                }
                printf("\n");
            }
        } else {
            if (n > 0 || err > 0) {
                printf("[%05d] REQA不整合: FIFO_Lv:%d | ErrorReg:0x%02X\n", loop_count, n, err);
            }
        }
        usleep(50000); 
    } // whileの閉じ括弧

    close(spi_fd);
    return 0;
} // mainの閉じ括弧（★不足していた箇所の完全補正）
