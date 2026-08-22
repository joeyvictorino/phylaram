use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::{Read, Result};
use std::path::Path;

pub fn calculate_logical_raw_sha256(raw_path: &Path, logical_size: u64) -> Result<String> {
    let mut file = File::open(raw_path)?;
    let mut hasher = Sha256::new();
    let mut buffer = vec![0u8; 1024 * 1024]; // 1 MiB chunk
    let mut total_read = 0u64;

    loop {
        let n = file.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        hasher.update(&buffer[..n]);
        total_read += n as u64;
    }

    if total_read < logical_size {
        let mut remaining_zeros = logical_size - total_read;
        let zero_buf = vec![0u8; 1024 * 1024];
        while remaining_zeros > 0 {
            let chunk = std::cmp::min(remaining_zeros, zero_buf.len() as u64) as usize;
            hasher.update(&zero_buf[..chunk]);
            remaining_zeros -= chunk as u64;
        }
    }

    let result = hasher.finalize();
    Ok(hex::encode(result))
}
