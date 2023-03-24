pub struct DeviceInfo {
    pub name: String,
    pub id: u64,
    pub total_size: u64,
    pub used_size: u64,
    pub block_szie: u64,
}

pub trait DeviceDriver {
    fn get_device_info(&self) -> Result<DeviceInfo, std::io::Error>;
    fn write() -> std::io::Result<()>;
    fn read(&self) -> std::io::Result<()>;
}
