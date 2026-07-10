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

	public List<IotLog> getAllLogs() {
		List<IotLog> logs = new ArrayList<>();
		String sql = "SELECT * FROM iot_log_data ORDER BY id DESC";

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
				logs.add(log);
			}
		}catch (SQLException e) {
			e.printStackTrace();
		}
		return logs;
	}
}
