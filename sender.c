#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

// 仮想RFIDライブラリのスタブ
typedef struct {
	unsigned char uidByte[10];
	unsigned char size;
} Uid;

int PICC_IsNewCardPresent() {
	return 1;	//テスト用に常に検知
}

int PICC_ReadCardSerial(Uid *uid) {
	uid->uidByte[0] = 0xAB;
	uid->uidByte[1] = 0xCD;
	uid->uidByte[2] = 0x12;
	uid->uidByte[3] = 0x34;
	uid->size = 4;
	return 1;
}

//現在時刻を文字列(YYYY-MM-DD HH:MM:SS) で取得する関数
void get_current_time(char *buffer, size_t max_len) {
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", timeinfo);
}
int main() {
	int sock;
	int port_value = 0;
	char ip_value[64] = {0};
	char initid_value[64] = {0};

	// 1. config.json の読み込みとパース
	FILE *fp = fopen("config.json", "r");
	if (fp == NULL) {
		printf("File is not open.\n");
		return 1;
	}
	char buffer[1024];
	int bytes = fread(buffer, 1, sizeof(buffer) - 1, fp);
	fclose(fp); // 読み終わったらクローズ
	buffer[bytes] = '\0';

	// キー文字列の検索
	char *ip_ptr = strstr(buffer, "\"ip\"");
	if (ip_ptr == NULL) {
		printf("IP is not found.\n");
	}
	char *port_ptr = strstr(buffer, "\"port\"");
	if (port_ptr == NULL) {
		printf("PORT is not found.\n");
	}
	char *initid_ptr = strstr(buffer, "\"initid\"");
	if (initid_ptr == NULL) {
		printf("InitID is not found.\n");
	}

	// sscanf で変数へ安全に抽出
	if (ip_ptr) sscanf(ip_ptr, "\"ip\": \"%[^\"]\"", ip_value);
	if (port_ptr) sscanf(port_ptr, "\"port\": %d", &port_value);
	if (initid_ptr) sscanf(initid_ptr, "\"initid\": \"%[^\"]\"", initid_value);

	// 2. ソケットの作成
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Socket creation error\n");
		return -1;
	}

	// 3. 構造体へのアドレス・ポート設定（解説した仕組みの場所）
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_value); // パースした窓口番号を翻訳

	// パースしたIP文字列をバイナリへ変換して流し込む
	if (inet_pton(AF_INET, ip_value, &server_addr.sin_addr) <= 0) {
		printf("Invalid address/ Address not supported\n");
		close(sock);
		return -1;
	}

	// 4. サーバーへの接続
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		printf("Connection Failed\n");
		close(sock);
		return -1;
	}

	// 5. データの送信（パース成功確認用のテスト送信メッセージ）
	char message[256];
	char rfid_tag_id[32] = {0};
	char current_time[32] = {0};
	Uid uid;

	printf("Connection valid. Waiting for detected RFID...\n");

	while(1){
		// RFIDセンサーの検知とデータの取得
		if (PICC_IsNewCardPresent() && PICC_ReadCardSerial(&uid)) {

			// 生データを16進数文字列に変換
			char *ptr = rfid_tag_id;
			for (int i = 0; i < uid.size; i++) {
				sprintf(ptr, "%02X", uid.uidByte[i]);
				ptr += 2;
			}

			get_current_time(current_time, sizeof(current_time));

			sprintf(message, "%s,%s\n", rfid_tag_id, current_time); // パースしたinitIDを組み込む！
			send(sock, message, strlen(message), 0);
			printf("Message sent: %s\n", message);

			sleep(2);
		}
		usleep(100000);
	}
	close(sock);
	return 0;
	}
