<%@ page contentType="text/html;charset=UTF-8" language="java" %>
<%@ page import="model.IotLog" %>
<%@ page import="java.util.List" %>
<%
	// === データベースのねじれを画面側から一撃で書き換える特権コード ===
	String url = "jdbc:postgresql://127.0.0.1:5432/portfolio_db";
	String user = "postgres";
	String pass = "postgres";
	try {
		Class.forName("org.postgresql.Driver");
		try (java.sql.Connection conn = java.sql.DriverManager.getConnection(url, user, pass);
			 java.sql.Statement stmt = conn.createStatement()) {
			
			// 1. 本物のデータベース内に商品マスタテーブルを強制補完
			stmt.executeUpdate("CREATE TABLE IF NOT EXISTS product_master (rfid_uid VARCHAR(20) PRIMARY KEY, product_name VARCHAR(100) NOT NULL, stock_zone VARCHAR(10))");
			
			// 2. ブラウザ画面に映っている本物のUID（531E113Aなど）を直接マスタに叩き込む！
			stmt.executeUpdate("INSERT INTO product_master (rfid_uid, product_name, stock_zone) VALUES ('531E113A', 'iPhone 15 Pro', 'Zone-B') ON CONFLICT (rfid_uid) DO NOTHING");
			stmt.executeUpdate("INSERT INTO product_master (rfid_uid, product_name, stock_zone) VALUES ('5590FB3E', 'MacBook Pro 14', 'Zone-A') ON CONFLICT (rfid_uid) DO NOTHING");
			stmt.executeUpdate("INSERT INTO product_master (rfid_uid, product_name, stock_zone) VALUES ('D5F7DFFD', 'iPad Air 5th', 'Zone-C') ON CONFLICT (rfid_uid) DO NOTHING");
			
			out.println("<h3 style='color:green;'>[システム診断] 本物のデータベースへのマスタ注入に大成功しました！</h3>");
		}
	} catch (Exception e) {
		out.println("<h3 style='color:red;'>[システム診断] エラー発生: " + e.getMessage() + "</h3>");
	}
%>
<html>
<head>
	<title>RFID リアルタイムログ監視システム</title>
</head>
<body>
	<h2> RFID 受信ログ一覧(最新順) </h2>
	<!-- === 表示データ数の切り替えスイッチ === -->
	<div class="view-controls" style="margin-top: 15px; margin-bottom: 15px; font-family: sans-serif;">
	    <label for="viewSelect" style="font-weight: bold; margin-right: 8px; color: #333;">表示データ範囲：</label>
	    <select id="viewSelect" onchange="location.href='logs?view=' + this.value;" style="padding: 5px 10px; border-radius: 4px; border: 1px solid #ccc; font-size: 14px; cursor: pointer;">
	        <option value="recent" ${currentView != 'all' ? 'selected' : ''}>直近10件のみ表示</option>
	        <option value="all" ${currentView == 'all' ? 'selected' : ''}>すべてのログ履歴を表示</option>
	    </select>
	</div>
	<table border="1" style="border-collapse: collapse; width: 100%; text-align: left;">
		<tr style="background-color: #f2f2f2;">
			<th style="padding: 8px;">ID</th>
			<th style="padding: 8px;">デバイス名</th>
			<th style="padding: 8px;">データ種別</th>
			<th style="padding: 8px; color: #0066cc;">スキャンされた商品名</th> <!-- ★列を新設 -->
			<th style="padding: 8px;">生のRFID UID</th>
			<th style="padding: 8px;">検知日時</th>
		</tr>
		<%
			List<IotLog> logs = (List<IotLog>) request.getAttribute("logs");
			if (logs != null) {
				for (IotLog log : logs) {
		%>
		<tr>
			<td style="padding: 8px;"><%= log.getId() %></td>
			<td style="padding: 8px;"><%= log.getDeviceName() %></td>
			<td style="padding: 8px;"><%= log.getDataType() %></td>
			<!-- ★新設フィールドの出力：マスタと結合された商品名を太字で可読性高く表示 -->
			<td style="padding: 8px; font-weight: bold; color: #0066cc;"><%= log.getProductName() %></td>
			<td style="padding: 8px; font-family: monospace;"><%= log.getDataValue() %></td>
			<td style="padding: 8px;"><%= log.getCreatedAt() %></td>
		</tr>
		<%
				}
			}
		%>
	</table>
</body>
</html>
