mod fake_device;
mod device_manager;

// use crate::fake_device::device_driver::DeviceDriver;
use crate::fake_device::device::BlockDevice;
use crate::fake_device::device::Device;
use crate::device_manager::DeviceManager;
use crate::device_manager::DeviceDriver;
use crate::device_manager::BlockDeviceDriver;

use bytes::{BytesMut, BufMut};
// use std::fmt::Write;
 
const DATA_BUFFER_POOL_CAPACITY: usize = 10;
const DATA_BUFFER_SIZE_IN_4KB: usize = 20;
 
fn main() {
    let mut device = BlockDevice::new("device01");
    device.init_buffer_pool();
 
    // host write buffer and its data
    let mut write_data = BytesMut::with_capacity(DATA_BUFFER_POOL_CAPACITY);
    write_data.put(&b"Hello Rust Wrold"[..]);
 
    // write to device
    let lpn = 0;
    device.write(lpn, DATA_BUFFER_SIZE_IN_4KB, &write_data);
 
    // read data from device.
    let read_data = BytesMut::zeroed(DATA_BUFFER_SIZE_IN_4KB);
    device.read(lpn, read_data);
 
    println!("remaining buffer size: {}", device.check_reamin_buffer());
 
    for n in 1..10 {
        let mut write_data = BytesMut::with_capacity(DATA_BUFFER_POOL_CAPACITY);
        write_data.put(&b"Hello Rust Wrold"[..]);
 
        // write to device
        let lpn = n;
        device.write(lpn, DATA_BUFFER_SIZE_IN_4KB, &write_data);
        println!("remaining buffer size: {}", device.check_reamin_buffer());
    }
 
    device.flush();
 
    println!("remaining buffer size: {}", device.check_reamin_buffer());
    println!("end");

    let mut device_manager = DeviceManager::new();
    let devices = ["device_01.txt", "device_02.txt"];
 
    for name in devices {
        device_manager.create(name.to_string());
        device_manager.print_device_list();
 
        let handle = device_manager.get_device_handle(name.to_string());
        let mut run = BlockDeviceDriver::new(handle);
        run.write();
        run.read();
    }
 
    for name in devices {
        device_manager.delete(name.to_string());
        device_manager.print_device_list();
    }
}