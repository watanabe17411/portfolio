<%@ page contentType="text/html;charset=UTF-7" kanguage="java" %>
<%@ page import="model.IotLog" %>
<%@ page import="java.util.List" %>
<html>
<head>
	<title>RFID リアルタイムログ監視システム</title>
</head>
<body>
	<h2> RFID 受信ログ一覧(最新順) </h2>
	<table border="1">
		<tr>
			<th>ID</th>
			<th>デバイス名</th>
			<th>データ種別</th>
			<th>データ値 (RFID UID, 検知日時)</th>
		</tr>
		<%
			//サーブレットから渡されたリストを受け取る
			List<IotLog> logs = (List<IotLog>) request.getAttribute("logs");
			if (logs != null) {
				for (IotLog log : logs) {
		%>
		<tr>
			<td><%= log.getId() %></td>
			<td><%= log.getDeviceName() %></td>
			<td><%= log.getDataType() %></td>
			<td><%= log.getDataValue() %></td>
		</tr>
		<%
				}
			}
		%>
	</table>
</body>
</html>
