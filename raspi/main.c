#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <linux/spi/spidev.h> // Linux標準SPIヘッダー

// --- MFRC522の物理レジスタ定義 ---
#define COMMAND_REG   0x01
#define BIT_FRAMING_REG 0x0D
#define FIFO_DATA_REG 0x09
#define FIFO_LEVEL_REG 0x0A
#define CONTROL_REG   0x0C

#define CMD_TRANSCEIVE 0x0C
#define PICC_REQIDL    0x26
#define PICC_ANTICOLL  0x93
#define PICC_HALT      0x50

// Linux標準のファイルディスクリプタ（ハンドル）
int spi_fd = -1;

// SPIレジスタ書き込み関数（標準spidev版）
void write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_data[2];
    uint8_t rx_data[2] = {0}; // 受信用クリア
    
    tx_data[0] = (reg << 1) & 0x7E;
    tx_data[1] = value;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = 2,
        .speed_hz = 4000000,     // 4MHz
        .bits_per_word = 8,
        .delay_usecs = 0,
    };
    
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI write error");
    }
}

// SPIレジスタ読み込み関数（標準spidev版）
uint8_t read_reg(uint8_t reg) {
    uint8_t tx_data[2];
    uint8_t rx_data[2] = {0};
    
    tx_data[0] = ((reg << 1) & 0x7E) | 0x80;
    tx_data[1] = 0x00;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = 2,
        .speed_hz = 4000000,
        .bits_per_word = 8,
        .delay_usecs = 0,
    };
    
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI read error");
        return 0;
    }
    return rx_data[1]; // 受信データの2バイト目を正確に返す
}

int main() {
    // 1. 物理ハードウェア（Linux標準 spidev）の初期化
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) {
        perror("【エラー】/dev/spidev0.0 のオープンに失敗しました。SPIを有効にしてください。");
        return 1;
    }
    
    // SPIモードを「Mode 0」に確定
    uint8_t mode = SPI_MODE_0;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("SPIモード設定失敗");
        close(spi_fd);
        return 1;
    }

    // MFRC522の内部アンテナを強制ON
    uint8_t tx_control = read_reg(0x14);
    if ((tx_control & 0x03) != 0x03) {
        write_reg(0x14, tx_control | 0x03);
    }
    write_reg(0x26, 0x70); // TxASKReg (100%ASK変調を維持)

    // 2. 通信・Javaバックエンドへのソケット常時接続設定
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000); // Receiver.javaの待ち受けポート
    
    // Java（Receiver）が動いているUbuntuのIPアドレスを指定
    serv_addr.sin_addr.s_addr = inet_addr("192.168.150.64");

    printf("Javaバックエンド（Receiver）に接続中...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Javaとのソケット通信の接続に失敗しました。\n");
        close(spi_fd);
        return 1;
    }
    printf("通信パイプライン開通！RFIDスキャン待機中...\n");

    // 各種基本レジスタの厳密な初期化
    write_reg(0x11, 0x3D); // ModeReg (CRC初期値)
    write_reg(0x2A, 0x8D); // TModeReg
    write_reg(0x2C, 0x30); // TReloadReg
    write_reg(0x24, 0x70); // ★修正：0x24(RFCfgReg)に0x70を書き込み、受信感度をMAX(48dB)に設定

    // --- 診断コード ---
    uint8_t version = read_reg(0x37); // 0x37番地（VersionReg）を読み出す
    printf("【物理チップ診断】MFRC522 バージョンコード: 0x%02X\n", version);

    // 3. ループ内での物理RFIDリアルタイムパースと常時送信
    uint8_t uid[5];
    char send_buf[128];

    while (1) {
        // --- ステップ1: カードを電波で叩き起こす (REQA) ---
        write_reg(COMMAND_REG, 0x00);      // 一度Idle状態にして前回の命令をリセット
        write_reg(FIFO_LEVEL_REG, 0x80);   // FlushBuffer
        write_reg(BIT_FRAMING_REG, 0x07);  // REQA用に下位7ビットモードを設定
        write_reg(FIFO_DATA_REG, 0x26);    // FIFOにREQAコマンドを充填
        
        // 修正：StartSend(0x80)をセットした状態でTransceive(0x0C)を叩き込み、即時バースト送信
        write_reg(COMMAND_REG, CMD_TRANSCEIVE);      
        write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80); 

        usleep(15000); // 15ミリ秒待機

        // --- ステップ2: 応答があれば、アンチコリジョンモードへ突入 ---
        uint8_t n = read_reg(FIFO_LEVEL_REG);
        if (n > 0) { 
            
            write_reg(COMMAND_REG, 0x00);    // アイドルに戻す
            write_reg(FIFO_LEVEL_REG, 0x80); // FlushBuffer
            write_reg(BIT_FRAMING_REG, 0x00); // 通常のバイト単位通信(0x00)にリセット
            write_reg(FIFO_DATA_REG, 0x93);  // 衝突防止コマンド
            write_reg(FIFO_DATA_REG, 0x20);  // 全ビット要求
            
            // 修正：StartSendをセットした状態でTransceiveを発行
            write_reg(COMMAND_REG, CMD_TRANSCEIVE);    
            write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80); 

            usleep(20000); // 20ミリ秒待機

            n = read_reg(FIFO_LEVEL_REG);
            if (n >= 5) { 
                
                uint8_t *ptr = uid;
                for (uint8_t i = 0; i < 5; i++) {
                    *ptr = read_reg(FIFO_DATA_REG);
                    ptr++; 
                }

                // 誤検知ストッパー
                if ((uid[0] == 0xFF && uid[1] == 0xFF) || (uid[0] == 0x00 && uid[1] == 0x00)) {
                    continue;
                }

                // 現在時刻を取得
                time_t now = time(NULL);

                // 1番目〜4番目の要素を正確に16進数パース
                snprintf(send_buf, sizeof(send_buf), "%02X%02X%02X%02X,%ld\n", 
                         uid[0], uid[1], uid[2], uid[3], now);

                printf("【物理RFID検知！】送信データ: %s", send_buf);

                // Java（Receiver）の有線常時接続ソケットへ直撃送信！
                if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
                    printf("送信失敗。パイプラインが切断されました。\n");
                    break;
                }
                
                sleep(2); // チャタリング防止
                
                // チップとカードを一度冷ます（Haltリセット）
                write_reg(COMMAND_REG, 0x00);
                write_reg(FIFO_LEVEL_REG, 0x80);
                write_reg(FIFO_DATA_REG, 0x50); 
                write_reg(FIFO_DATA_REG, 0x00);
                write_reg(COMMAND_REG, CMD_TRANSCEIVE);
                write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80);
                usleep(10000);
            }
        }
        usleep(100000); // 100ms周期でスキャンを回す
    }

    // 4. クリーンアップ
    close(sock);
    close(spi_fd);
    return 0;
}
