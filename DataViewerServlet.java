import jakarta.servlet.ServletException;
import jakarta.servlet.annotation.WebServlet;
import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.*;
import java.sql.*;          
import java.util.Properties; 

@WebServlet("/view")
public class DataViewerServlet extends HttpServlet {
    @Override
    protected void doGet(HttpServletRequest request, HttpServletResponse response) 
            throws ServletException, IOException {
        
        // --- 準備：ブラウザへの出力用ペンを作る ---
        response.setContentType("text/html;charset=UTF-8");
        PrintWriter out = response.getWriter();

        // --- 表示モード（mode）を取得（デフォルトは直近10件） ---
        String mode = request.getParameter("mode");
        if (mode == null || mode.isEmpty()) {
            mode = "recent"; 
        }

        // --- 設定ファイル（db.properties）を読み込む ---
        Properties props = new Properties();
        try (InputStream input = getClass().getClassLoader().getResourceAsStream("db.properties")) {
            if (input == null) {
                out.print("<p style='color:red;'>エラー: db.properties が見つかりません。</p>");
                return;
            }
            props.load(input);
        } catch (IOException e) {
            out.print("<p style='color:red;'>設定ファイルの読み込み失敗: " + e.getMessage() + "</p>");
            return;
        }

        // --- 読み込んだ設定からURLを組み立てる ---
        String dbHost = props.getProperty("db.host");
        String dbPort = props.getProperty("db.port");
        String dbName = props.getProperty("db.name");
        String dbUser = props.getProperty("db.user");
        String dbPass = props.getProperty("db.password");
        String dbUrl = String.format("jdbc:postgresql://%s:%s/%s", dbHost, dbPort, dbName);

        // --- HTMLの骨組みを出力 ---
        out.print("<html lang='ja'>");
        out.print("<head>");
        out.print("<meta charset='UTF-8'>");
        out.print("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
        out.print("<title>ポートフォリオ</title>");
        out.print("</head>");
        out.print("<body>");
        out.print("<h1>📊 IoT センサー受信データ最新ログ</h1>");
        out.print("<p>ポートフォリオ作成</p>");
        
        // --- UI表示切り替えリンク生成 ---
        out.print("<div style='margin-bottom: 20px; font-size: 1.1em;'>");
        out.print("<strong>表示設定: </strong>");
        if (mode.equals("all")) {
            out.print("<a href='view?mode=recent' style='text-decoration: none; color: #0066cc;'>➔ 直近10件のみ表示に切り替える</a> | ");
            out.print("<strong style='color: #ff3333; background-color: #ffe6e6; padding: 2px 6px; border-radius: 4px;'>すべてのデータを表示中</strong>");
        } else {
            out.print("<strong style='color: #228b22; background-color: #e6ffe6; padding: 2px 6px; border-radius: 4px;'>直近10件を表示中</strong> | ");
            out.print("<a href='view?mode=all' style='text-decoration: none; color: #0066cc;'>➔ すべてのデータを表示に切り替える</a>");
        }
        out.print("</div>");

        // --- 【商品名追加】テーブルヘッダー出力 ---
        out.print("<table border='1' cellpadding='10'>");
        out.print("<tr bgcolor='#cccccc'><th>ID</th><th>デバイス名</th><th>データ型</th><th>生のUID</th><th>商品名</th><th>受信時刻</th></tr>");

        // --- モードに応じて外部結合（LEFT JOIN）SQLを動的に決定 ---
        String sql;
        if (mode.equals("all")) {
            sql = "SELECT log.*, mast.product_name " +
                  "FROM iot_log_data log " +
                  "LEFT JOIN product_master mast ON log.data_value = mast.rfid_uid " +
                  "ORDER BY log.id DESC";
        } else {
            sql = "SELECT log.*, mast.product_name " +
                  "FROM iot_log_data log " +
                  "LEFT JOIN product_master mast ON log.data_value = mast.rfid_uid " +
                  "ORDER BY log.id DESC LIMIT 10";
        }

        // --- データベース処理 ---
        try {
            Class.forName("org.postgresql.Driver");
            
            try (Connection conn = DriverManager.getConnection(dbUrl, dbUser, dbPass);
                 Statement stmt = conn.createStatement();
                 ResultSet rs = stmt.executeQuery(sql)) {
                
                while (rs.next()) {
                    // マスタ登録状態のチェックガードロジック
                    String prodName = rs.getString("product_name");
                    if (prodName == null) {
                        prodName = "<span style='color:gray;'>未登録の商品タグ</span>";
                    } else {
                        prodName = "<strong style='color:#0066cc;'>" + prodName + "</strong>";
                    }

                    out.print("<tr>");
                    out.print("<td>" + rs.getInt("id") + "</td>");
                    out.print("<td>" + rs.getString("device_name") + "</td>");
                    out.print("<td>" + rs.getString("data_type") + "</td>");
                    out.print("<td>" + rs.getString("data_value") + "</td>"); // 生のUID
                    out.print("<td>" + prodName + "</td>");                   // 結合されたマスタ商品名
                    out.print("<td>" + rs.getTimestamp("created_at") + "</td>");
                    out.print("</tr>");
                }
            }
        } catch (Exception e) {
            out.print("<p style='color:red;'>DBエラー発生: " + e.getMessage() + "</p>");
            e.printStackTrace();
        }

        // --- HTMLの閉じタグ ---
        out.print("</table>");        
        out.print("</body>");
        out.print("</html>");
    }
}
