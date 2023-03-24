use std::collections::HashMap;
use bytes::{BytesMut};
use std::vec::Vec;
use std::collections::VecDeque;
use std::fs::File;
use std::io::Write;
 
const DATA_BUFFER_POOL_CAPACITY: usize = 10;
const DATA_BUFFER_SIZE_IN_4KB: usize = 20;
 
#[derive(Debug, Clone)]
struct DataDesc {
    id: u32,
    buffer: BytesMut,
}
 
impl DataDesc {
    pub fn new(index: u32, buffer: BytesMut) -> DataDesc {
        DataDesc {
            id: index,
            buffer: buffer,
        }
    }
 
    pub fn copy_buffer(&mut self, data: &BytesMut) {
        self.buffer[..data.len()].copy_from_slice(data);
        println!("written data {:?}", self.buffer);
    }
 
    pub fn get_index(&mut self) -> u32 {
        self.id
    }
}

pub trait Device {
    fn write(&mut self, lpn: u64, count: usize, host_buffer: &BytesMut) -> std::io::Result<()>;
    fn read(&mut self, lpn: u64, buf: BytesMut) -> std::io::Result<()>;
}
 
pub struct BlockDevice<'a> {
    device_name: &'a str,
    free_buf_list: VecDeque<u32>,
    used_buf_list: Vec<u32>,
    buffer_pool: Vec<DataDesc>,
    lpn_hash: HashMap<u64, u32>,
}
 
impl BlockDevice<'_> {
    pub fn new(input_name: &str) -> BlockDevice {
        BlockDevice {
            device_name : input_name,
            free_buf_list : VecDeque::with_capacity(DATA_BUFFER_POOL_CAPACITY),
            used_buf_list : Vec::with_capacity(DATA_BUFFER_POOL_CAPACITY),
            buffer_pool : Vec::with_capacity(DATA_BUFFER_POOL_CAPACITY),
            lpn_hash : HashMap::new()
        }
    }
 
    pub fn get_device_name(&self) -> &str {
        self.device_name.clone()
    }
 
    pub fn init_buffer_pool(&mut self) {
        for i in 0..DATA_BUFFER_POOL_CAPACITY as u32 {
            let buffer = BytesMut::zeroed(DATA_BUFFER_SIZE_IN_4KB);
            // note: didn't work. expected length 4096, but actual length was 10.
            // let mut buffer = BytesMut::with_capaticy(DATA_BUFFER_SIZE_IN_4KB);
            // buffer.put(&[0; DATA_BUFFER_POOL_CAPACITY][..]);
            let data_buffer = DataDesc::new(i, buffer);
            self.buffer_pool.push(data_buffer);
            self.free_buf_list.push_back(i);
        }
    }
 
    pub fn check_reamin_buffer(&mut self) -> usize {
        self.free_buf_list.len()
    }
 
    pub fn flush(&mut self) {
        let file = File::create("flushed_data.txt");
 
        for n in &self.used_buf_list {
            let src = &self.buffer_pool[*n as usize];
            // file write
            file.as_ref().expect("invalid file").write_fmt(format_args!("LPN:{}", n));
            file.as_ref().expect("invalid file").write_all(&src.buffer[..]);
 
            // clear src
            self.free_buf_list.push_back(*n);
        }
 
        self.used_buf_list.clear();
    }
}
 
impl Device for BlockDevice<'_> {
    fn write(&mut self, lpn: u64, count: usize, host_buffer: &BytesMut) -> std::io::Result<()> {
        match self.free_buf_list.pop_front() {
            Some(index) => {
                self.buffer_pool[index as usize].copy_buffer(host_buffer);
                self.lpn_hash.insert(lpn, index);
                self.used_buf_list.push(index);
            },
            None => {
                panic!("Invalid data buffer!");
            }
        }
 
        Ok(())
    }
 
    fn read(&mut self, lpn: u64, mut buf: BytesMut) -> std::io::Result<()> {
        match self.lpn_hash.get(&lpn) {
            Some(id) => {
                let data_buffer_id: usize = (*id).try_into().unwrap();
                let src = &self.buffer_pool[data_buffer_id];
                buf[..src.buffer.len()].copy_from_slice(&src.buffer);
                println!("read data {:?}", buf);
            },
            None => {
                println!("unmapped lpn {}", lpn);
            }
        }
 
        Ok(())
    }
}