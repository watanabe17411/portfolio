package model;

import java.sql.Timestamp;

public class IotLog {
	private int id;
	private String deviceName;
	private String dataType;
	private String dataValue;
	private Timestamp createdAt;
	private String productName; // ★自動倉庫用：商品名フィールド

	// 既存の5引数コンストラクタ（完全維持）
	public IotLog(int id, String deviceName, String dataType, String dataValue, Timestamp createdAt) {
		this.id = id;
		this.deviceName = deviceName;
		this.dataType = dataType;
		this.dataValue = dataValue;
		this.createdAt = createdAt;
	}

	// ゲッター / セッター群
	public int getId() { return id; }
	public String getDeviceName() { return deviceName; }
	public String getDataType() { return dataType; }
	public String getDataValue() { return dataValue; }
	public Timestamp getCreatedAt() { return createdAt; }

	// 商品名用のゲッター・セッター
	public String getProductName() { return productName; }
	public void setProductName(String productName) { this.productName = productName; }
}
