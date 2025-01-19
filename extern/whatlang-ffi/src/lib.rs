use std::ffi::{c_char, CStr};

#[repr(C)]
pub struct WlInfo {
    pub lang: u8,
    pub confidence: f32,
}

#[repr(C)]
pub enum WlError {
    Ok = 0,
    MalformedString = 1,
    DetectionFailed = 2,
}

#[no_mangle]
pub unsafe extern "C" fn wl_detect(string: *const c_char, result: *mut WlInfo) -> WlError {
    match detect(CStr::from_ptr(string)) {
        Ok(info) => {
            *result = info;
            WlError::Ok
        }
        Err(err) => err
    }
}

#[inline]
fn detect(string: &CStr) -> Result<WlInfo, WlError> {
    let string = string.to_str().map_err(|_| WlError::MalformedString)?;
    let info = whatlang::detect(string).ok_or(WlError::DetectionFailed)?;
    Ok(WlInfo {
        lang: info.lang() as u8,
        confidence: info.confidence() as f32,
    })
}
