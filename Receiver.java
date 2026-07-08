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

        // 3. 既存のソケットサーバー通信処理
        System.out.println("Java Server started. Waiting for connection on port " + socketPort + "...");

        try (ServerSocket serverSocket = new ServerSocket(socketPort);
             Socket clientSocket = serverSocket.accept();
             BufferedReader in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()))) {

            System.out.println("Connected from C client!");

            String receivedMessage = in.readLine();
            System.out.println("Received: " + receivedMessage);

            // カンマで分割 (例: "RasPi_01", "TEMP", "26.5")
            String[] data = receivedMessage.split(",");
            String deviceName = data[0];    // -> "RFID_READER_01"
            String rfidTagId = data[1];      // -> "E280113020007461" (RFIDタグID)
            String dataValue = "N/A";

            // 4. データベースへ接続・保存 (変数 dbUrl, dbUser, dbPass を使用)
            System.out.println("Connecting to Database... -> " + dbUrl);
            Class.forName("org.postgresql.Driver");
            
            try (Connection conn = DriverManager.getConnection(dbUrl, dbUser, dbPass)) {
                String sql = "INSERT INTO iot_log_data (device_name, data_type, data_value) VALUES (?, ?, ?)";
                try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                    pstmt.setString(1, deviceName);
                    pstmt.setString(2, rfidTagId);
                    pstmt.setString(3, dataValue);
                    
                    int rows = pstmt.executeUpdate();
                    if (rows > 0) {
                        System.out.println("➔ [SQL成功] データベースに正常に保存されました！");
                    }
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
