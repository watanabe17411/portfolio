#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	FILE *fp = fopen("config.json", "r");
	if(fp == NULL){
		printf("Faile is cannot open.\n");
		return 1;
	}
	char buffer[1024];
	int bytes = fread(buffer, 1, sizeof(buffer) - 1, fp);
	buffer[bytes] = '\0';
	printf("%s",buffer);
	// PORTの抽出
	char *port_ptr = strstr(buffer, "\"port\"");
	if (port_ptr == NULL) {
	    printf("Port is not found.\n");
	    return 1;
	}
	// IPアドレスの抽出
	char *ip_ptr = strstr(buffer, "\"ip\"");
	if (ip_ptr == NULL) {
	    printf("Ip is not found.\n");
	    return 1;
	}
	// initIDの抽出
	char *initid_ptr = strstr(buffer, "\"initid\"");
	if (initid_ptr == NULL) {
	    printf("Initid is not found.\n");
	    return 1;
	}
	// 数値の抽出
	// PORT 
	int port_value = 0;
	sscanf(port_ptr, "\"port\": %d", &port_value);
	printf("抽出したポート番号: %d\n", port_value);
	// IP
	char ip_value[64] = {0};
	sscanf(ip_ptr, "\"ip\": \"%[^\"]\"", ip_value);
	printf("抽出したIPアドレス: %s\n", ip_value);
	// initID
	char initid_value[64] = {0};
	sscanf(initid_ptr, "\"initid\": \"%[^\"]\"" , initid_value);
	printf("抽出したinitID: %s\n", initid_value);

	fclose(fp);
	return 0;
}
