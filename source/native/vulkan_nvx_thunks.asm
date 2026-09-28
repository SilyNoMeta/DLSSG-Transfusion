; Detours of NVSDK_NGX_VULKAN_Init_Ext2, one per provider slot (vulkan_nvx.h).
;
; Init_Ext2(appId, path, instance, physicalDevice, device, gipa, gdpa, sdk, features)
; On entry gipa is at [rsp+30h] and gdpa at [rsp+38h]: stack arguments belong
; to the callee, so they are rewritten in place. The thunk then jumps to the
; original, which returns straight to NGX with NGX's own return address (the
; provider validates its caller; a call from here would change it).
.code

extern VulkanNvxRouteInit : proc
extern g_vulkanNvxInitTrampolines : qword

INIT_THUNK macro index
public VulkanNvxInitThunk&index&
VulkanNvxInitThunk&index& proc frame
    mov qword ptr [rsp+8h], rcx
    mov qword ptr [rsp+10h], rdx
    mov qword ptr [rsp+18h], r8
    mov qword ptr [rsp+20h], r9
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    lea rcx, [rsp+28h+30h]
    lea rdx, [rsp+28h+38h]
    mov r8d, index
    call VulkanNvxRouteInit
    mov rcx, qword ptr [rsp+28h+8h]
    mov rdx, qword ptr [rsp+28h+10h]
    mov r8, qword ptr [rsp+28h+18h]
    mov r9, qword ptr [rsp+28h+20h]
    add rsp, 28h
    jmp qword ptr [g_vulkanNvxInitTrampolines + index * 8]
VulkanNvxInitThunk&index& endp
endm

INIT_THUNK 0
INIT_THUNK 1
INIT_THUNK 2
INIT_THUNK 3

end
