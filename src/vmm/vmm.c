#include "vmm/vmm.h"
#include "hardware/registers.h"
#include "hardware/vmcs.h"
#include "hardware/types.h"
#include "hardware/vmx.h"
#include "hardware/error_codes.h"
#include "hardware/msr.h"
#include "lib/log.h"
#include "lib/utils.h"

#define CR4_VMX_ENABLE (1 << 13)
#define CR0_NE_ENABLE (1 << 5)

#define IA32_FEATURE_CONTROL_LOCK_BIT (1 << 0)
#define IA32_FEATURE_CONTROL_ENABLE_VMXON_INSIDE_SMX (1 << 1)
#define IA32_FEATURE_CONTROL_ENABLE_VMXON_OUTSIDE_SMX (1 << 2)

#define VMXON_REGION_ADDRESS 0x10000
#define VMCS_REGION_ADDRESS 0x11000
#define PAGE_FRAME_SIZE 0x1000

vm_instruction_error_t check_vm_instruction_error();


void enter_vmx_root() {    
    // Note that PE & PG should be 0 with "unrestricted guest".
    write_cr0((read_cr0() | CR0_NE_ENABLE | read_msr(MSR_IA32_VMX_CR0_FIXED0)) & read_msr(MSR_IA32_VMX_CR0_FIXED1));
    write_cr4((read_cr4() | CR4_VMX_ENABLE | read_msr(MSR_IA32_VMX_CR4_FIXED0)) & read_msr(MSR_IA32_VMX_CR4_FIXED1));

    int feature_control_msr = read_msr(MSR_IA32_FEATURE_CONTROL);
    LOG_DEBUG("IA32_FEATURE_CONTROL: %b", feature_control_msr);

    if (feature_control_msr & IA32_FEATURE_CONTROL_LOCK_BIT) {
        // meaning the bios initialized the msr and locked it
        if (feature_control_msr & IA32_FEATURE_CONTROL_ENABLE_VMXON_INSIDE_SMX) {
            LOG_INFO("support of vmxon inside smx operation");
        } else if (feature_control_msr & IA32_FEATURE_CONTROL_ENABLE_VMXON_OUTSIDE_SMX) {
            LOG_INFO("support of vmxon outside smx operation");
        } else {
            PANIC("no support for vmxon instruction");
        }
    } else {
        write_msr(
            MSR_IA32_FEATURE_CONTROL,
            feature_control_msr | 
            IA32_FEATURE_CONTROL_LOCK_BIT |
            IA32_FEATURE_CONTROL_ENABLE_VMXON_INSIDE_SMX | 
            IA32_FEATURE_CONTROL_ENABLE_VMXON_OUTSIDE_SMX
            );

        LOG_INFO("support of vmxon inside and outside of smx operation");
    }
    

    initialize_vmx_regions((char *)VMXON_REGION_ADDRESS, (char *)VMCS_REGION_ADDRESS);
    
    ASSERT(vmxon((void *)VMXON_REGION_ADDRESS) == VM_SUCCESS);
    ASSERT(vmclear((void *)VMCS_REGION_ADDRESS) == VM_SUCCESS);
    ASSERT(vmptrld((void *)VMCS_REGION_ADDRESS) == VM_SUCCESS);
    
    LOG_INFO("entered VMX-root operation");
    
    LOG_DEBUG("current VM-instruction error: %s", VM_INSTRUCTION_ERROR_STRINGS[check_vm_instruction_error()]);
}

void initialize_vmx_regions(char* vmxon_region, char* vmcs_region) {

    memset((void *)vmxon_region, 0, PAGE_FRAME_SIZE);
    memset((void *)vmcs_region, 0, PAGE_FRAME_SIZE);

    *(dword_t *)vmxon_region = read_msr(MSR_IA32_VMX_BASIC);
    *(dword_t *)vmcs_region = read_msr(MSR_IA32_VMX_BASIC);
}

void configure_vmcs() {
    // vm execution control fields
    
    dword_t default1 = read_msr(MSR_IA32_VMX_PINBASED_CTLS);
    dword_t default0 = read_msr(MSR_IA32_VMX_PINBASED_CTLS) >> 32;

    if ((read_msr(MSR_IA32_VMX_BASIC) & (1LL << 55)) >> 55) {
        default1 = read_msr(MSR_IA32_VMX_TRUE_PINBASED_CTLS);
        default0 = read_msr(MSR_IA32_VMX_TRUE_PINBASED_CTLS) >> 32;
        LOG_DEBUG("defualt1 at IA32_VMX_TRUE_PINBASED_CTLS: %b", default1);
        // bits 1, 2, 4 must contain inside default1
        ASSERT(default1 >> 1 && default1 >> 2 && default1 >> 4);
    }
    vmwrite(VMCS_PIN_BASED_VM_EXEC_CONTROL, default1 & default0);

}

vm_instruction_error_t check_vm_instruction_error() {
    qword_t error_code = UNDEFINE_VM_INSTRUCTION_ERROR;
    vmread(VMCS_VM_INSTRUCTION_ERROR, &error_code);

    return (vm_instruction_error_t)error_code;
}