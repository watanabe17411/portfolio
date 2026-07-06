#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" 	//自分自身を指定
#define PORT 8500		//受信待ちのポート番号

int main(){
	int sock;
	struct sockaddr_in server_addr;
	//今回作成した共通DBテーブルに合わせ、「送信元、データ型、値」を模擬
	char *message = "RasPi_01, TEMP, 26.5\n";

	//1.ソケットの作成
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("Socket creation error\n");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

	printf("Connecting to port %d...\n", PORT);

	//2.サーバーに接続
	if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){ 
		printf("Connection failed, Start Java server first.\n");
		return -1;
	}

	//3.データの送信
	printf("Sending data: %s", message);
	send(sock, message, strlen(message), 0);

	close(sock);
	printf("Disconnected.\n");
	return 0;
}
