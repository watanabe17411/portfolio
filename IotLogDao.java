package dao;

import model.IotLog;
import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class IotLogDao {
	private String dbUrl;
	private String dbUser;
	private String dbPass;

	public IotLogDao(String dbUrl, String dbUser, String dbPass) {
		this.dbUrl = dbUrl;
		this.dbUser = dbUser;
		this.dbPass = dbPass;
	}

	// 1. 全件取得メソッド（空白・ノイズ完全抹殺 LEFT JOIN 結合版）
	public List<IotLog> getAllLogs() {
		List<IotLog> logs = new ArrayList<>();
		try { Class.forName("org.postgresql.Driver"); } catch (ClassNotFoundException e) { e.printStackTrace(); }
		
		// data_valueをコンマで割り、前後の空白・改行(TRIM)を消し去って product_master と結合
		String sql = "SELECT l.*, COALESCE(p.product_name, '未登録の商品タグ') AS product_name "
		           + "FROM iot_log_data l "
		           + "LEFT JOIN product_master p ON TRIM(BOTH ' ' FROM SPLIT_PART(l.data_value, ',', 1)) = p.rfid_uid "
		           + "ORDER BY l.id DESC";

		try (Connection conn = DriverManager.getConnection(dbUrl, dbUser, dbPass);
			PreparedStatement pstmt = conn.prepareStatement(sql);
			ResultSet rs = pstmt.executeQuery()) {

			while (rs.next()) {
				IotLog log = new IotLog(
					rs.getInt("id"),
					rs.getString("device_name"),
					rs.getString("data_type"),
					rs.getString("data_value"),
					rs.getTimestamp("created_at")
				);
				log.setProductName(rs.getString("product_name"));
				logs.add(log);
			}
		} catch (SQLException e) { e.printStackTrace(); }
		return logs;
	}

	// 2. 直近10件取得メソッド（空白・ノイズ完全抹殺 LEFT JOIN 結合版）
	public List<IotLog> getRecent10Logs() {
		List<IotLog> logs = new ArrayList<>();
		try { Class.forName("org.postgresql.Driver"); } catch (ClassNotFoundException e) { e.printStackTrace(); }
		
		String sql = "SELECT l.*, COALESCE(p.product_name, '未登録の商品タグ') AS product_name "
		           + "FROM iot_log_data l "
		           + "LEFT JOIN product_master p ON TRIM(BOTH ' ' FROM SPLIT_PART(l.data_value, ',', 1)) = p.rfid_uid "
		           + "ORDER BY l.id DESC LIMIT 10";

		try (Connection conn = DriverManager.getConnection(dbUrl, dbUser, dbPass);
			PreparedStatement pstmt = conn.prepareStatement(sql);
			ResultSet rs = pstmt.executeQuery()) {

			while (rs.next()) {
				IotLog log = new IotLog(
					rs.getInt("id"),
					rs.getString("device_name"),
					rs.getString("data_type"),
					rs.getString("data_value"),
					rs.getTimestamp("created_at")
				);
				log.setProductName(rs.getString("product_name"));
				logs.add(log);
			}
		} catch (SQLException e) { e.printStackTrace(); }
		return logs;
	}
}
