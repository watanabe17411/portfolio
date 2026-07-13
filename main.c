#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <bcm2835.h>

// --- MFRC522の物理レジスタ定義 ---
#define COMMAND_REG   0x01
#define BIT_FRAMING_REG 0x0D
#define FIFO_DATA_REG 0x09
#define FIFO_LEVEL_REG 0x0A
#define CONTROL_REG   0x0C

#define CMD_TRANSCEIVE 0x0C
#define PICC_REQIDL    0x26
#define PICC_ANTICOLL  0x93
#define PICC_HALT 0x50

// 疑似ではなく、本物のSPIレジスタを叩くための書き込み関数
void write_reg(uint8_t reg, uint8_t value) {
    uint8_t data[2];
    data[0] = (reg << 1) & 0x7E;
    data[1] = value;
    bcm2835_spi_transfern((char*)data, 2);
}

// 物理レジスタからの読み込み関数
uint8_t read_reg(uint8_t reg) {
    uint8_t data[2];
    data[0] = ((reg << 1) & 0x7E) | 0x80;
    data[1] = 0x00;
    bcm2835_spi_transfern((char*)data, 2);
    return data[1];
}

int main() {
    // 1. 物理ハードウェア（GPIO/SPI）の初期化
    if (!bcm2835_init()) {
        printf("bcm2835初期化失敗。sudoで実行してください。\n");
        return 1;
    }
    
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); // 通信速度設定
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                   // 24番ピン(CS0)を使用
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    // MFRC522の内部アンテナを強制ON
    uint8_t tx_control = read_reg(0x14);
    if ((tx_control & 0x03) != 0x03) {
        write_reg(0x14, tx_control | 0x03);
	write_reg(0x26, 0x70);
    }

    // 2. 通信・Javaバックエンドへのソケット常時接続設定
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000); // Receiver.javaの待ち受けポート
    
    // Java（Receiver）が動いているUbuntuのIPアドレスを指定
    serv_addr.sin_addr.s_addr = inet_addr("192.168.150.64\n");

    printf("Javaバックエンド（Receiver）に接続中...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Javaとのソケット通信の接続に失敗しました。\n");
        bcm2835_spi_end();
        bcm2835_close();
        return 1;
    }
    printf("通信パイプライン開通！RFIDスキャン待機中...\n");

    // 3. ループ内での物理RFIDリアルタイムパースと常時送信
    uint8_t uid[5];
    char send_buf[128];

    write_reg(0x11, 0x3D); // ModeReg
    write_reg(0x2A, 0x8D); // TModeReg
    write_reg(0x2C, 0x30); // TReloadReg
    write_reg(0x26, 0x70); // RFCfgReg (感度MAX)

    // --- 診断コード ---
    uint8_t version = read_reg(0x37); // 0x37番地（VersionReg）を読み出す
    printf("【物理チップ診断】MFRC522 バージョンコード: 0x%02X\n", version);

     while (1) {
        // MFRC522へFIFO経由でアンチコリジョン（衝突防止・ID要求）コマンドをシリアル送信
        write_reg(COMMAND_REG, 0x00); // Idle
        write_reg(FIFO_LEVEL_REG, 0x80); // Flush FIFO
      	write_reg(FIFO_DATA_REG, PICC_HALT);
	write_reg(FIFO_DATA_REG, PICC_ANTICOLL);
        write_reg(FIFO_DATA_REG, 0x20);
        write_reg(COMMAND_REG, CMD_TRANSCEIVE);
        write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80); // Start Send

        usleep(20000); // チップの応答を20ミリ秒待つ

        uint8_t n = read_reg(FIFO_LEVEL_REG);
        if (n >= 5) { // FIFOの中に生バイト配列（UID）が届いているか確認

            uint8_t *ptr = uid;
            for (uint8_t i = 0; i < 5; i++) {
                *ptr = read_reg(FIFO_DATA_REG);
                ptr++; // ポインタを前に移動させながら生バイト配列を格納
            }

            // 現在時刻（エポックタイム秒）を取得
            time_t now = time(NULL);

            // 生バイト配列を16進数文字列（"ABCD1234"）にパースし、日時とカンマ結合
            snprintf(send_buf, sizeof(send_buf), "%02X%02X%02X%02X,%ld\n", 
                     uid[0], uid[1], uid[2], uid[3], now);

            printf("RFID検知！送信データ: %s", send_buf);

            // Java（Receiver）の常時接続ソケットへ永続送信
            if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
                printf("送信失敗。パイプラインが切断されました。\n");
                break;
            }
	   // データ送信後、チップを初期状態に戻してFIFO(メモリ)を空にする
	    write_reg(COMMAND_REG, 0x00);
	    write_reg(FIFO_LEVEL_REG, 0x80);

            // チャタリング防止（連続インサートを防ぐために2秒スリープ）
            sleep(2);
        }
        usleep(100000); // 100ミリ秒周期でスキャンループ
    }
    while (1) {
        // --- ステップ1: カードを電波で叩き起こす (REQA: Request A) ---
        write_reg(COMMAND_REG, 0x00);      // Idle状態
        write_reg(FIFO_LEVEL_REG, 0x80);   // FIFOをクリア
        write_reg(FIFO_DATA_REG, 0x26);    // REQAコマンド (0x26) を装填
        write_reg(COMMAND_REG, 0x0C);      // Transceive (送信開始)
        write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80); // 送信スタート

        usleep(15000); // 15ミリ秒、カードの応答を待つ

        // --- ステップ2: 応答があれば、アンチコリジョン（ID要求）モードへ突入 ---
        uint8_t n = read_reg(FIFO_LEVEL_REG);
        if (n > 0) { // 何かしらカードが電波に反応した場合
            
            write_reg(COMMAND_REG, 0x00);    // 一度Idle状態
            write_reg(FIFO_LEVEL_REG, 0x80); // FIFOを綺麗にフラッシュ
            write_reg(FIFO_DATA_REG, 0x93);  // アンチコリジョンコマンド (0x93)
            write_reg(FIFO_DATA_REG, 0x20);  // NVB (すべてのビットを要求)
            write_reg(COMMAND_REG, 0x0C);    // Transceive (IDの吸い出し開始)
            write_reg(BIT_FRAMING_REG, read_reg(BIT_FRAMING_REG) | 0x80); // スタート

            usleep(20000); // 20ミリ秒、生バイト配列（UID）が届くのを待つ

            n = read_reg(FIFO_LEVEL_REG);
            if (n >= 5) { // FIFOに本物の5バイトのUIDが着弾したか確認
                
                // 【宗恩さんのポインタパース構造】
                uint8_t *ptr = uid;
                for (uint8_t i = 0; i < 5; i++) {
                    *ptr = read_reg(FIFO_DATA_REG);
                    ptr++; // ポインタをインクリメントしながら格納
                }

                // もし壊れた値（FFFFFFFFや00000000）なら誤検知としてスキップ
                if ((uid[0] == 0xFF && uid[1] == 0xFF) || (uid[0] == 0x00 && uid[1] == 0x00)) {
                    continue;
                }

                // 現在時刻（エポックタイム秒）を取得
                time_t now = time(NULL);

                // 生バイト配列を16進数文字列にパースし、日時とカンマ結合
                snprintf(send_buf, sizeof(send_buf), "%02X%02X%02X%02X,%ld\n", 
                         uid, uid, uid, uid, now);

                printf("【物理RFID検知！】送信データ: %s", send_buf);

                // Java（Receiver）の有線常時接続ソケットへ送信
                if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
                    printf("送信失敗。パイプラインが切断されました。\n");
                    break;
                }
                
                // チャタリング防止と、チップを完全に冷ますための2秒スリープ
                sleep(2);
                
                // 送り終わったらカードに休止命令 (Halt) を送ってリセット
                write_reg(COMMAND_REG, 0x00);
                write_reg(FIFO_LEVEL_REG, 0x80);
                write_reg(FIFO_DATA_REG, 0x50); // PICC_HALT
                write_reg(FIFO_DATA_REG, 0x00);
                write_reg(COMMAND_REG, 0x0C);
                usleep(10000);
            }
        }
        usleep(100000); // 100ミリ秒周期でスキャンループ
    }

    // 4. クリーンアップ
    close(sock);
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}
