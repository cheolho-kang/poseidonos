use log::{info, trace, warn};
use std::collections::HashMap;
use std::fs::File;
use std::io::prelude::*;
 
pub struct DeviceManager {
    device_list: HashMap<String, File>,
}
 
impl DeviceManager {
    pub fn new() -> DeviceManager {
        DeviceManager {
            device_list: HashMap::new(),
        }
    }
 
    pub fn create(&mut self, name: String) -> std::io::Result<()> {
        let file = File::create(name.clone())?;
        self.device_list.insert(name, file);
        Ok(())
    }
 
    pub fn delete(&mut self, name: String) -> std::io::Result<()> {
        self.device_list.remove(&name);
        Ok(())
    }
 
    pub fn get_device_handle(&mut self, name: String) -> &File {
        &self.device_list[&name]
    }
 
    pub fn print_device_list(&mut self) {
        info!("number of online devices : {}", self.device_list.len());
 
        for (name, fd) in &self.device_list {
            info!("{name}: \"{:?}\"", fd);
        }
    }
}
 
pub trait DeviceDriver {
    fn write(&mut self) -> std::io::Result<()>;
    fn read(&mut self) -> std::io::Result<()>;
}
 
pub struct BlockDeviceDriver<'a> {
    device_handle : &'a File,
}
 
impl BlockDeviceDriver<'_> {
    pub fn new(handle: &File) -> BlockDeviceDriver<> {
        BlockDeviceDriver {
            device_handle : handle
        }
    }
}
 
impl DeviceDriver for BlockDeviceDriver<'_> {
    fn write(&mut self) -> std::io::Result<()> {
        self.device_handle.write_all(b"Hello, world!")?;
        Ok(())
    }
 
    fn read(&mut self) -> std::io::Result<()> {
        let mut contents = String::new();
        self.device_handle.read_to_string(&mut contents)?;
        info!("read string : {}", contents);
        assert_eq!(contents, "Hello, world!");
        Ok(())
    }
}