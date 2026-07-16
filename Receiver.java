import java.io.*;
import java.net.*;
import java.sql.*;
import java.util.Properties;

public class Receiver {
    
    public static void main(String[] args) {
        // 1. 設定ファイルの動的ロード
        Properties props = new Properties();
        try (InputStream input = new FileInputStream("db.properties")) {
            props.load(input);
        } catch (IOException e) {
            System.err.println("設定ファイルの読み込みに失敗: " + e.getMessage());
            return;
        }

        // 2. 外部設定を変数化
        String dbHost = props.getProperty("db.host");
        String dbPort = props.getProperty("db.port");
        String dbName = props.getProperty("db.name");
        String dbUser = props.getProperty("db.user");
        String dbPass = props.getProperty("db.password");
        int socketPort = Integer.parseInt(props.getProperty("socket.port", "5000"));

        // JDBC接続URLの組み立て
        String dbUrl = String.format("jdbc:postgresql://%s:%s/%s", dbHost, dbPort, dbName);

        // 3. データベースへ接続・保存準備
        System.out.println("Connecting to Database... -> " + dbUrl);
        
        try {
            Class.forName("org.postgresql.Driver");
            
            try (Connection conn = DriverManager.getConnection(dbUrl, dbUser, dbPass);
                 PreparedStatement pstmt = conn.prepareStatement("INSERT INTO iot_log_data (device_name, data_type, data_value) VALUES (?, ?, ?)")) {
                
                // 4. ソケットサーバー通信の開始
                System.out.println("Java Server started. Waiting for connection on port " + socketPort + "...");

                try (ServerSocket serverSocket = new ServerSocket(socketPort);
                     Socket clientSocket = serverSocket.accept();
                     BufferedReader in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()))) {

                    System.out.println("Connected from C client!");

                    String receivedMessage;
                    // 5. データが届く限り、通信もDB接続も維持したままループ
	                    while ((receivedMessage = in.readLine()) != null) {
	                        System.out.println("Received: " + receivedMessage);

	                        // カンマで分割
	                        String[] data = receivedMessage.split(",");
						if (data.length >= 2){
	                        String rfidTagId = data[0].trim();     // -> "ABCD1234"
	                        String detectedTime = data[1];	// -> "2026-07-10 11:14:02"

	                        // 外側で1回だけ生成した pstmt を使い回す
	                        pstmt.setString(1, "RASPBERRY_PI_RFID");
	                        pstmt.setString(2, "RFID_UID");
	                        pstmt.setString(3, rfidTagId);
	                        
	                        int rows = pstmt.executeUpdate();
	                        if (rows > 0) {
	                            System.out.println("➔ [SQL成功] データベースに正常に保存されました！");
	                        }
	                    } else {
								System.err.println("受信デートのフォーマットが不正です: " + receivedMessage);
						} 
					}
               }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
