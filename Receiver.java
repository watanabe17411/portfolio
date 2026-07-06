import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.ServerSocket;
import java.net.Socket;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;

public class Receiver {
    private static final int PORT = 8500;
    // データベースへの接続情報
    private static final String DB_URL = "jdbc:postgresql://localhost:51432/postgres"; // 通常5432ですがポリテク環境用に調整する場合あり
    private static final String DB_USER = "postgres";
    private static final String DB_PASS = ""; // パスワードなし、または設定したもの

    public static void main(String[] args) {
        System.out.println("Java Server started. Waiting for connection...");

        try (ServerSocket serverSocket = new ServerSocket(PORT);
             Socket clientSocket = serverSocket.accept();
             BufferedReader in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()))) {

            System.out.println("Connected from C client!");

            String receivedMessage = in.readLine();
            System.out.println("Received: " + receivedMessage);

            // カンマで分割 (例: "RasPi_01", "TEMP", "26.5")
            String[] data = receivedMessage.split(",");
            String deviceName = data[0];
            String dataType = data[1];
            String dataValue = data[2];

            // データベースへ保存 (SQLの発行)
            System.out.println("Connecting to Database...");
            Class.forName("org.postgresql.Driver");
            try (Connection conn = DriverManager.getConnection(DB_URL, DB_USER, DB_PASS)) {
                String sql = "INSERT INTO iot_log_data (device_name, data_type, data_value) VALUES (?, ?, ?)";
                try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
                    pstmt.setString(1, deviceName);
                    pstmt.setString(2, dataType);
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
