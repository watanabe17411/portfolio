package model;

import java.sql.Timestamp;

public class IotLog {
	private int id;
	private String deviceName;
	private String dataType;
	private String dataValue;
	private Timestamp createdAt;

	public IotLog(int id, String deviceName, String dataType, String dataValue, Timestamp createdAt) {
		this.id = id;
		this.deviceName = deviceName;
		this.dataType = dataType;
		this.dataValue = dataValue;
		this.createdAt = createdAt;
	}

	public int getId() { return id; }
	public String getDeviceName() { return deviceName; }
	public String getDataType() { return dataType; }
	public String getDataValue() { return dataValue; }
	public Timestamp getCreatedAt() { return createdAt; }
}
