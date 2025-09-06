// Aurora NVMe Driver - Rust Implementation
// Comprehensive NVMe driver with admin and I/O queue management

use core::ptr;
use core::mem;
use core::slice;

// NVMe constants
const NVME_CAP_MQES_SHIFT: u8 = 0;
const NVME_CAP_MQES_MASK: u64 = 0xffff;
const NVME_CAP_CQR_SHIFT: u8 = 16;
const NVME_CAP_CQR_MASK: u64 = 1;
const NVME_CAP_AMS_SHIFT: u8 = 17;
const NVME_CAP_AMS_MASK: u64 = 3;
const NVME_CAP_TO_SHIFT: u8 = 24;
const NVME_CAP_TO_MASK: u64 = 0xff;
const NVME_CAP_DSTRD_SHIFT: u8 = 32;
const NVME_CAP_DSTRD_MASK: u64 = 0xf;
const NVME_CAP_NSSRS_SHIFT: u8 = 36;
const NVME_CAP_NSSRS_MASK: u64 = 1;
const NVME_CAP_CSS_SHIFT: u8 = 37;
const NVME_CAP_CSS_MASK: u64 = 0xff;
const NVME_CAP_BPS_SHIFT: u8 = 45;
const NVME_CAP_BPS_MASK: u64 = 1;
const NVME_CAP_MPSMIN_SHIFT: u8 = 48;
const NVME_CAP_MPSMIN_MASK: u64 = 0xf;
const NVME_CAP_MPSMAX_SHIFT: u8 = 52;
const NVME_CAP_MPSMAX_MASK: u64 = 0xf;

// NVMe register offsets
const NVME_REG_CAP: u64 = 0x00;
const NVME_REG_VS: u64 = 0x08;
const NVME_REG_INTMS: u64 = 0x0c;
const NVME_REG_INTMC: u64 = 0x10;
const NVME_REG_CC: u64 = 0x14;
const NVME_REG_CSTS: u64 = 0x1c;
const NVME_REG_NSSR: u64 = 0x20;
const NVME_REG_AQA: u64 = 0x24;
const NVME_REG_ASQ: u64 = 0x28;
const NVME_REG_ACQ: u64 = 0x30;
const NVME_REG_CMBLOC: u64 = 0x38;
const NVME_REG_CMBSZ: u64 = 0x3c;

// NVMe controller configuration
const NVME_CC_ENABLE: u32 = 1 << 0;
const NVME_CC_CSS_NVM: u32 = 0 << 4;
const NVME_CC_MPS_SHIFT: u8 = 7;
const NVME_CC_AMS_RR: u32 = 0 << 11;
const NVME_CC_SHN_NONE: u32 = 0 << 14;
const NVME_CC_SHN_NORMAL: u32 = 1 << 14;
const NVME_CC_SHN_ABRUPT: u32 = 2 << 14;
const NVME_CC_SHN_MASK: u32 = 3 << 14;
const NVME_CC_IOSQES: u32 = 6 << 16;
const NVME_CC_IOCQES: u32 = 4 << 20;

// NVMe controller status
const NVME_CSTS_RDY: u32 = 1 << 0;
const NVME_CSTS_CFS: u32 = 1 << 1;
const NVME_CSTS_NSSRO: u32 = 1 << 4;
const NVME_CSTS_PP: u32 = 1 << 5;
const NVME_CSTS_SHST_NORMAL: u32 = 0 << 2;
const NVME_CSTS_SHST_OCCUR: u32 = 1 << 2;
const NVME_CSTS_SHST_CMPLT: u32 = 2 << 2;
const NVME_CSTS_SHST_MASK: u32 = 3 << 2;

// NVMe admin commands
const NVME_ADMIN_DELETE_SQ: u8 = 0x00;
const NVME_ADMIN_CREATE_SQ: u8 = 0x01;
const NVME_ADMIN_GET_LOG_PAGE: u8 = 0x02;
const NVME_ADMIN_DELETE_CQ: u8 = 0x04;
const NVME_ADMIN_CREATE_CQ: u8 = 0x05;
const NVME_ADMIN_IDENTIFY: u8 = 0x06;
const NVME_ADMIN_ABORT_CMD: u8 = 0x08;
const NVME_ADMIN_SET_FEATURES: u8 = 0x09;
const NVME_ADMIN_GET_FEATURES: u8 = 0x0a;
const NVME_ADMIN_ASYNC_EVENT: u8 = 0x0c;
const NVME_ADMIN_NS_MGMT: u8 = 0x0d;
const NVME_ADMIN_ACTIVATE_FW: u8 = 0x10;
const NVME_ADMIN_DOWNLOAD_FW: u8 = 0x11;
const NVME_ADMIN_FORMAT_NVM: u8 = 0x80;
const NVME_ADMIN_SECURITY_SEND: u8 = 0x81;
const NVME_ADMIN_SECURITY_RECV: u8 = 0x82;

// NVMe I/O commands
const NVME_CMD_FLUSH: u8 = 0x00;
const NVME_CMD_WRITE: u8 = 0x01;
const NVME_CMD_READ: u8 = 0x02;
const NVME_CMD_WRITE_UNCOR: u8 = 0x04;
const NVME_CMD_COMPARE: u8 = 0x05;
const NVME_CMD_WRITE_ZEROES: u8 = 0x08;
const NVME_CMD_DSM: u8 = 0x09;
const NVME_CMD_RESV_REGISTER: u8 = 0x0d;
const NVME_CMD_RESV_REPORT: u8 = 0x0e;
const NVME_CMD_RESV_ACQUIRE: u8 = 0x11;
const NVME_CMD_RESV_RELEASE: u8 = 0x15;

// NVMe command structure
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct NvmeCommand {
    pub cdw0: u32,
    pub nsid: u32,
    pub cdw2: u32,
    pub cdw3: u32,
    pub metadata: u64,
    pub prp1: u64,
    pub prp2: u64,
    pub cdw10: u32,
    pub cdw11: u32,
    pub cdw12: u32,
    pub cdw13: u32,
    pub cdw14: u32,
    pub cdw15: u32,
}

// NVMe completion entry
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct NvmeCompletion {
    pub result: u32,
    pub rsvd: u32,
    pub sq_head: u16,
    pub sq_id: u16,
    pub command_id: u16,
    pub status: u16,
}

// NVMe identify controller structure (simplified)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct NvmeIdCtrl {
    pub vid: u16,
    pub ssvid: u16,
    pub sn: [u8; 20],
    pub mn: [u8; 40],
    pub fr: [u8; 8],
    pub rab: u8,
    pub ieee: [u8; 3],
    pub cmic: u8,
    pub mdts: u8,
    pub cntlid: u16,
    pub ver: u32,
    pub rtd3r: u32,
    pub rtd3e: u32,
    pub oaes: u32,
    pub ctratt: u32,
    pub rrls: u16,
    pub rsvd102: [u8; 9],
    pub cntrltype: u8,
    pub fguid: [u8; 16],
    pub crdt1: u16,
    pub crdt2: u16,
    pub crdt3: u16,
    pub rsvd134: [u8; 122],
    pub oacs: u16,
    pub acl: u8,
    pub aerl: u8,
    pub frmw: u8,
    pub lpa: u8,
    pub elpe: u8,
    pub npss: u8,
    pub avscc: u8,
    pub apsta: u8,
    pub wctemp: u16,
    pub cctemp: u16,
    pub mtfa: u16,
    pub hmpre: u32,
    pub hmmin: u32,
    pub tnvmcap: [u8; 16],
    pub unvmcap: [u8; 16],
    pub rpmbs: u32,
    pub edstt: u16,
    pub dsto: u8,
    pub fwug: u8,
    pub kas: u16,
    pub hctma: u16,
    pub mntmt: u16,
    pub mxtmt: u16,
    pub sanicap: u32,
    pub hmminds: u32,
    pub hmmaxd: u16,
    pub nsetidmax: u16,
    pub endgidmax: u16,
    pub anatt: u8,
    pub anacap: u8,
    pub anagrpmax: u32,
    pub nanagrpid: u32,
    pub pels: u32,
    pub rsvd356: [u8; 156],
    pub sqes: u8,
    pub cqes: u8,
    pub maxcmd: u16,
    pub nn: u32,
    pub oncs: u16,
    pub fuses: u16,
    pub fna: u8,
    pub vwc: u8,
    pub awun: u16,
    pub awupf: u16,
    pub nvscc: u8,
    pub nwpc: u8,
    pub acwu: u16,
    pub rsvd534: [u8; 2],
    pub sgls: u32,
    pub mnan: u32,
    pub rsvd544: [u8; 224],
    pub subnqn: [u8; 256],
    pub rsvd1024: [u8; 768],
    pub ioccsz: u32,
    pub iorcsz: u32,
    pub icdoff: u16,
    pub ctrattr: u8,
    pub msdbd: u8,
    pub rsvd1804: [u8; 244],
    pub psd: [u8; 1024],
    pub vs: [u8; 1024],
}

// NVMe identify namespace structure (simplified)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct NvmeIdNs {
    pub nsze: u64,
    pub ncap: u64,
    pub nuse: u64,
    pub nsfeat: u8,
    pub nlbaf: u8,
    pub flbas: u8,
    pub mc: u8,
    pub dpc: u8,
    pub dps: u8,
    pub nmic: u8,
    pub rescap: u8,
    pub fpi: u8,
    pub dlfeat: u8,
    pub nawun: u16,
    pub nawupf: u16,
    pub nacwu: u16,
    pub nabsn: u16,
    pub nabo: u16,
    pub nabspf: u16,
    pub noiob: u16,
    pub nvmcap: [u8; 16],
    pub npwg: u16,
    pub npwa: u16,
    pub npdg: u16,
    pub npda: u16,
    pub nows: u16,
    pub rsvd74: [u8; 18],
    pub anagrpid: u32,
    pub rsvd96: [u8; 3],
    pub nsattr: u8,
    pub nvmsetid: u16,
    pub endgid: u16,
    pub nguid: [u8; 16],
    pub eui64: [u8; 8],
    pub lbaf: [u32; 16],
    pub rsvd192: [u8; 192],
    pub vs: [u8; 3712],
}

// NVMe queue structure
#[repr(C)]
pub struct NvmeQueue {
    pub sq_cmds: *mut NvmeCommand,
    pub cq_cmds: *mut NvmeCompletion,
    pub sq_dma_addr: u64,
    pub cq_dma_addr: u64,
    pub sq_tail: u16,
    pub cq_head: u16,
    pub cq_phase: u8,
    pub q_depth: u16,
    pub qid: u16,
    pub cq_vector: u8,
}

// NVMe controller structure
#[repr(C)]
pub struct NvmeCtrl {
    pub bar: *mut u8,
    pub admin_q: NvmeQueue,
    pub queues: [Option<NvmeQueue>; 64],
    pub queue_count: u16,
    pub max_qid: u16,
    pub db_stride: u32,
    pub ctrl_config: u32,
    pub page_size: u32,
    pub cap: u64,
    pub vs: u32,
    pub online_queues: u16,
    pub max_transfer_shift: u8,
    pub shutdown_timeout: u16,
    pub kato: u16,
}

impl NvmeCtrl {
    pub fn new(bar: *mut u8) -> Self {
        Self {
            bar,
            admin_q: unsafe { mem::zeroed() },
            queues: [None; 64],
            queue_count: 0,
            max_qid: 0,
            db_stride: 0,
            ctrl_config: 0,
            page_size: 4096,
            cap: 0,
            vs: 0,
            online_queues: 0,
            max_transfer_shift: 0,
            shutdown_timeout: 0,
            kato: 0,
        }
    }

    pub fn read_reg32(&self, offset: u64) -> u32 {
        unsafe {
            ptr::read_volatile((self.bar.add(offset as usize)) as *const u32)
        }
    }

    pub fn write_reg32(&self, offset: u64, value: u32) {
        unsafe {
            ptr::write_volatile((self.bar.add(offset as usize)) as *mut u32, value);
        }
    }

    pub fn read_reg64(&self, offset: u64) -> u64 {
        unsafe {
            ptr::read_volatile((self.bar.add(offset as usize)) as *const u64)
        }
    }

    pub fn write_reg64(&self, offset: u64, value: u64) {
        unsafe {
            ptr::write_volatile((self.bar.add(offset as usize)) as *mut u64, value);
        }
    }

    pub fn initialize(&mut self) -> Result<(), i32> {
        // Read controller capabilities
        self.cap = self.read_reg64(NVME_REG_CAP);
        self.vs = self.read_reg32(NVME_REG_VS);
        
        // Calculate doorbell stride
        self.db_stride = 1 << (((self.cap >> NVME_CAP_DSTRD_SHIFT) & NVME_CAP_DSTRD_MASK) + 2);
        
        // Set page size
        let mps_min = (self.cap >> NVME_CAP_MPSMIN_SHIFT) & NVME_CAP_MPSMIN_MASK;
        let mps_max = (self.cap >> NVME_CAP_MPSMAX_SHIFT) & NVME_CAP_MPSMAX_MASK;
        let page_shift = 12 + mps_min;
        self.page_size = 1 << page_shift;
        
        // Disable controller
        self.write_reg32(NVME_REG_CC, 0);
        
        // Wait for controller to be ready
        let mut timeout = 1000;
        while (self.read_reg32(NVME_REG_CSTS) & NVME_CSTS_RDY) != 0 && timeout > 0 {
            timeout -= 1;
        }
        
        if timeout == 0 {
            return Err(-1);
        }
        
        // Setup admin queue
        self.setup_admin_queue()?;
        
        // Enable controller
        let cc = NVME_CC_ENABLE | NVME_CC_CSS_NVM | NVME_CC_AMS_RR |
                 NVME_CC_SHN_NONE | NVME_CC_IOSQES | NVME_CC_IOCQES |
                 ((page_shift - 12) << NVME_CC_MPS_SHIFT) as u32;
        
        self.ctrl_config = cc;
        self.write_reg32(NVME_REG_CC, cc);
        
        // Wait for controller ready
        timeout = 1000;
        while (self.read_reg32(NVME_REG_CSTS) & NVME_CSTS_RDY) == 0 && timeout > 0 {
            timeout -= 1;
        }
        
        if timeout == 0 {
            return Err(-2);
        }
        
        Ok(())
    }
    
    fn setup_admin_queue(&mut self) -> Result<(), i32> {
        // Allocate admin queue memory (simplified - would use proper DMA allocation)
        let sq_size = 64 * mem::size_of::<NvmeCommand>();
        let cq_size = 64 * mem::size_of::<NvmeCompletion>();
        
        // In real implementation, these would be DMA-coherent allocations
        self.admin_q.sq_cmds = ptr::null_mut(); // Would allocate DMA memory
        self.admin_q.cq_cmds = ptr::null_mut(); // Would allocate DMA memory
        self.admin_q.sq_dma_addr = 0; // Would be real DMA address
        self.admin_q.cq_dma_addr = 0; // Would be real DMA address
        
        self.admin_q.q_depth = 64;
        self.admin_q.qid = 0;
        self.admin_q.sq_tail = 0;
        self.admin_q.cq_head = 0;
        self.admin_q.cq_phase = 1;
        
        // Set admin queue attributes
        let aqa = ((64 - 1) << 16) | (64 - 1);
        self.write_reg32(NVME_REG_AQA, aqa);
        
        // Set admin queue addresses
        self.write_reg64(NVME_REG_ASQ, self.admin_q.sq_dma_addr);
        self.write_reg64(NVME_REG_ACQ, self.admin_q.cq_dma_addr);
        
        Ok(())
    }
    
    pub fn submit_admin_cmd(&mut self, cmd: &NvmeCommand) -> Result<u16, i32> {
        if self.admin_q.sq_cmds.is_null() {
            return Err(-1);
        }
        
        let tail = self.admin_q.sq_tail;
        
        unsafe {
            ptr::write_volatile(self.admin_q.sq_cmds.add(tail as usize), *cmd);
        }
        
        self.admin_q.sq_tail = (tail + 1) % self.admin_q.q_depth;
        
        // Ring doorbell
        let doorbell_offset = 0x1000 + (0 * 2 * self.db_stride);
        self.write_reg32(doorbell_offset as u64, self.admin_q.sq_tail as u32);
        
        Ok(tail)
    }
    
    pub fn identify_controller(&mut self, data: *mut NvmeIdCtrl) -> Result<(), i32> {
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        
        cmd.cdw0 = NVME_ADMIN_IDENTIFY as u32;
        cmd.nsid = 0;
        cmd.prp1 = data as u64; // In real implementation, would be DMA address
        cmd.cdw10 = 1; // Controller identify
        
        self.submit_admin_cmd(&cmd)?;
        
        // Wait for completion (simplified)
        // In real implementation, would wait for interrupt or poll completion queue
        
        Ok(())
    }
    
    pub fn identify_namespace(&mut self, nsid: u32, data: *mut NvmeIdNs) -> Result<(), i32> {
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        
        cmd.cdw0 = NVME_ADMIN_IDENTIFY as u32;
        cmd.nsid = nsid;
        cmd.prp1 = data as u64; // In real implementation, would be DMA address
        cmd.cdw10 = 0; // Namespace identify
        
        self.submit_admin_cmd(&cmd)?;
        
        Ok(())
    }
    
    pub fn create_io_queue(&mut self, qid: u16, qsize: u16) -> Result<(), i32> {
        if qid == 0 || qid >= 64 {
            return Err(-1);
        }
        
        // Create completion queue first
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        cmd.cdw0 = NVME_ADMIN_CREATE_CQ as u32;
        cmd.cdw10 = ((qsize - 1) << 16) | qid as u32;
        cmd.cdw11 = 1; // Physically contiguous
        
        self.submit_admin_cmd(&cmd)?;
        
        // Create submission queue
        cmd = unsafe { mem::zeroed() };
        cmd.cdw0 = NVME_ADMIN_CREATE_SQ as u32;
        cmd.cdw10 = ((qsize - 1) << 16) | qid as u32;
        cmd.cdw11 = (qid << 16) | 1; // Associated CQ ID and physically contiguous
        
        self.submit_admin_cmd(&cmd)?;
        
        Ok(())
    }
    
    pub fn read_data(&mut self, nsid: u32, lba: u64, blocks: u16, buffer: *mut u8) -> Result<(), i32> {
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        
        cmd.cdw0 = NVME_CMD_READ as u32;
        cmd.nsid = nsid;
        cmd.prp1 = buffer as u64; // In real implementation, would be DMA address
        cmd.cdw10 = (lba & 0xffffffff) as u32;
        cmd.cdw11 = (lba >> 32) as u32;
        cmd.cdw12 = (blocks - 1) as u32; // 0-based
        
        // Submit to I/O queue (simplified - using admin queue for now)
        self.submit_admin_cmd(&cmd)?;
        
        Ok(())
    }
    
    pub fn write_data(&mut self, nsid: u32, lba: u64, blocks: u16, buffer: *const u8) -> Result<(), i32> {
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        
        cmd.cdw0 = NVME_CMD_WRITE as u32;
        cmd.nsid = nsid;
        cmd.prp1 = buffer as u64; // In real implementation, would be DMA address
        cmd.cdw10 = (lba & 0xffffffff) as u32;
        cmd.cdw11 = (lba >> 32) as u32;
        cmd.cdw12 = (blocks - 1) as u32; // 0-based
        
        // Submit to I/O queue (simplified - using admin queue for now)
        self.submit_admin_cmd(&cmd)?;
        
        Ok(())
    }
    
    pub fn flush(&mut self, nsid: u32) -> Result<(), i32> {
        let mut cmd: NvmeCommand = unsafe { mem::zeroed() };
        
        cmd.cdw0 = NVME_CMD_FLUSH as u32;
        cmd.nsid = nsid;
        
        self.submit_admin_cmd(&cmd)?;
        
        Ok(())
    }
}

// C interface functions
#[no_mangle]
pub extern "C" fn nvme_create_controller(bar: *mut u8) -> *mut NvmeCtrl {
    let ctrl = Box::new(NvmeCtrl::new(bar));
    Box::into_raw(ctrl)
}

#[no_mangle]
pub extern "C" fn nvme_destroy_controller(ctrl: *mut NvmeCtrl) {
    if !ctrl.is_null() {
        unsafe {
            let _ = Box::from_raw(ctrl);
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_initialize_controller(ctrl: *mut NvmeCtrl) -> i32 {
    if ctrl.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).initialize() {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_admin_identify(ctrl: *mut NvmeCtrl, data: *mut NvmeIdCtrl) -> i32 {
    if ctrl.is_null() || data.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).identify_controller(data) {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_identify_namespace(ctrl: *mut NvmeCtrl, nsid: u32, data: *mut NvmeIdNs) -> i32 {
    if ctrl.is_null() || data.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).identify_namespace(nsid, data) {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_read_blocks(ctrl: *mut NvmeCtrl, nsid: u32, lba: u64, blocks: u16, buffer: *mut u8) -> i32 {
    if ctrl.is_null() || buffer.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).read_data(nsid, lba, blocks, buffer) {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_write_blocks(ctrl: *mut NvmeCtrl, nsid: u32, lba: u64, blocks: u16, buffer: *const u8) -> i32 {
    if ctrl.is_null() || buffer.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).write_data(nsid, lba, blocks, buffer) {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

#[no_mangle]
pub extern "C" fn nvme_flush_namespace(ctrl: *mut NvmeCtrl, nsid: u32) -> i32 {
    if ctrl.is_null() {
        return -1;
    }
    
    unsafe {
        match (*ctrl).flush(nsid) {
            Ok(()) => 0,
            Err(e) => e,
        }
    }
}

// Panic handler for no_std environment
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

// Global allocator error handler
#[alloc_error_handler]
fn alloc_error_handler(_layout: core::alloc::Layout) -> ! {
    loop {}
}

extern crate alloc;
use alloc::boxed::Box;
