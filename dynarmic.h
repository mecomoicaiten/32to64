#pragma once

#include <vector>

#ifdef DYNARMIC_MASTER
#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/config.h>

#include <dynarmic/interface/exclusive_monitor.h>
#else
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>

#include <dynarmic/exclusive_monitor.h>
#endif

#include "khash.h"
#include "filesystem.h"
#include "32bit.h"

// TSB (thread local variables)
#define ARM_REG_C13_C0_3 113
#define ARM_REG_SP Reg::SP
#define ARM_REG_PC Reg::PC

#define PAGE_TABLE_ADDRESS_SPACE_BITS 36
#define DYN_PAGE_BITS 12 // 4k
#define DYN_PAGE_SIZE (1ULL << DYN_PAGE_BITS)
#define DYN_PAGE_MASK (DYN_PAGE_SIZE-1)
#define UC_PROT_WRITE 2

#define ALIGN_DYN_SIZE(len) (((DYN_PAGE_SIZE-1)&len) ? ((len+DYN_PAGE_SIZE) & ~(DYN_PAGE_SIZE-1)):len)
#define ALIGN_SIZE(len) (((PAGE_SIZE-1)&len) ? ((len+PAGE_SIZE) & ~(PAGE_SIZE-1)):len)

#define DYLD_PROCESS_INFO_NOTIFY_LOAD_ID 0x1000
#define DYLD_PROCESS_INFO_NOTIFY_UNLOAD_ID 0x2000
#define DYLD_PROCESS_INFO_NOTIFY_MAIN_ID 0x3000

// cpsr flags
#define A32_BIT 4
#define THUMB_BIT 5
#define NEGATIVE_BIT 31
#define ZERO_BIT 30
#define CARRY_BIT 29
#define OVERFLOW_BIT 28
#define MODE_MASK 0x1f

#define PRE_CALLBACK_SYSCALL_NUMBER 0x8866
#define POST_CALLBACK_SYSCALL_NUMBER 0x8888
#define DARWIN_SWI_SYSCALL 0x80

#define LC32HaltReasonSVC Dynarmic::HaltReason::UserDefined1
#define LC32HaltReasonRetFromGuest Dynarmic::HaltReason::UserDefined2

class Reg {
public:
  enum RegEnum {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    SP = R13,
    LR = R14,
    PC = R15,
    INVALID_REG = 99
  };
};

class ExtReg {
public:
  enum ExtRegEnum {
    // clang-format off
    S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15, S16, S17, S18, S19, S20, S21, S22, S23, S24, S25, S26, S27, S28, S29, S30, S31,
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15, D16, D17, D18, D19, D20, D21, D22, D23, D24, D25, D26, D27, D28, D29, D30, D31,
    Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15
    // clang-format on
  };
};

struct guest_file_mapping {
    const char *name;
    uint64_t hostAddr;
    uint32_t start;
    uint32_t end;
};
extern int guestMappingLen;
extern guest_file_mapping guestMappings[1000];

typedef struct memory_page {
  void *addr;
  int perms;
} *t_memory_page;

KHASH_MAP_INIT_INT64(memory, t_memory_page)

using Vector = std::array<std::uint64_t, 2>;

typedef struct context32 {
  std::array<std::uint32_t, 16> regs;
  std::array<std::uint32_t, 64> extRegs;
  std::uint32_t cpsr;
  std::uint32_t fpscr;
  std::uint32_t uro;
} *t_context32;


#include "arm_dynarmic_cp15.h"

class DynarmicCpsr {
public:
    DynarmicCpsr(Dynarmic::A32::Jit *cpu)
        : cpu{cpu} {}

    bool hasBit(int offset) {
        return ((cpu->Cpsr() >> offset) & 1) == 1;
    }

    void setBit(int offset) {
        int mask = 1 << offset;
        cpu->SetCpsr(cpu->Cpsr() | mask);
    }

    void clearBit(int offset) {
        int mask = ~(1 << offset);
        cpu->SetCpsr(cpu->Cpsr() & mask);
    }

    bool isA32() {
        return hasBit(A32_BIT);
    }

    bool isThumb() {
        return hasBit(THUMB_BIT);
    }

    bool isNegative() {
        return hasBit(NEGATIVE_BIT);
    }

    void setNegative(bool on) {
        if (on) {
            setBit(NEGATIVE_BIT);
        } else {
            clearBit(NEGATIVE_BIT);
        }
    }

    bool isZero() {
        return hasBit(ZERO_BIT);
    }

    void setZero(bool on) {
        if (on) {
            setBit(ZERO_BIT);
        } else {
            clearBit(ZERO_BIT);
        }
    }

    bool hasCarry() {
        return hasBit(CARRY_BIT);
    }

    void setCarry(bool on) {
        if (on) {
            setBit(CARRY_BIT);
        } else {
            clearBit(CARRY_BIT);
        }
    }

    bool isOverflow() {
        return hasBit(OVERFLOW_BIT);
    }

    void setOverflow(bool on) {
        if (on) {
            setBit(OVERFLOW_BIT);
        } else {
            clearBit(OVERFLOW_BIT);
        }
    }

    int getMode() {
        return cpu->Cpsr() & MODE_MASK;
    }

    int getEL() {
        return (cpu->Cpsr() >> 2) & 3;
    }

    Dynarmic::A32::Jit *cpu;
};

__BEGIN_DECLS

class DynarmicCallbacks32;
typedef struct {
  khash_t(memory) *memory;
  size_t num_page_table_entries;
  void **page_table;
  union {
    DynarmicCallbacks32 *cb;
    Dynarmic::A32::UserCallbacks *ucb;
  };
  Dynarmic::ExclusiveMonitor *monitor;
  LC32Filesystem *fs;
  u32 guest_dlsym, guest_LC32InvokeGuestC;
  dyld_all_image_infos_32 *dyld_info_section;
} dynarmic;

typedef struct {
  Dynarmic::A32::Jit *jit;
  DynarmicCpsr *cpsr;
} dynarmic_thread;

// Handles
extern dynarmic sharedHandle;
extern __thread dynarmic_thread threadHandle;

char *get_memory_page(u64 vaddr);
void *get_memory(u64 vaddr);

bool Dynarmic_nativeInitialize();
void Dynarmic_nativeDestroy();
int Dynarmic_munmap(u64 address, u64 size);
u32 Dynarmic_direct_mmap(u32 address, u32 size, int protection, int flags, void *src, u64 off);
u32 Dynarmic_mmap(u32 address, u32 size, int protection, int flags, int fildes, u64 off, u64 mask = DYN_PAGE_MASK);
int Dynarmic_mprotect(u64 address, u64 size, int perms);
int Dynarmic_mem_1write(u64 address, u64 size, char* src);
int Dynarmic_mem_1read(u64 address, u64 size, char* dest);
int Dynarmic_reg_1write(int index, u32 value);
u32 Dynarmic_reg_1read(int index);
int Dynarmic_reg_1read_1cpsr();
int Dynarmic_reg_1write_1cpsr(int value);
int Dynarmic_reg_1write_1c13_1c0_13(int value);
int Dynarmic_emu_1start(u32 pc);
int Dynarmic_emu_1stop();
void* Dynarmic_context_1alloc();
void Dynarmic_context_1restore(t_context32 ctx);
void Dynarmic_context_1save(t_context32 ctx);
void Dynarmic_free(void *ctx);

u64 LC32Dlsym(u32 guest_name, bool isFunction);
u64 LC32GetHostObject(u32 guest_self, u32 guest_class, bool returnClass);
u64 LC32GetHostSelector(u32 guest_selector);
u64 LC32InvokeHostSelector(u64 host_self, u64 host_cmd, u64 va_args);
u32 LC32HostToGuestCopyClassName(u32 guest_output, size_t length, u64 host_object);

__END_DECLS

#define U64_MASK (sizeof(u64)-1)
// create a string in the guest stack pointer
class DynarmicGuestStackString {
public:
    ~DynarmicGuestStackString() {
        threadHandle.jit->Regs()[Reg::SP] += totalLen;
    }

    DynarmicGuestStackString(const char *hostPtr) {
        // align to 8-byte
        totalLen = (strlen(hostPtr) + 1 + U64_MASK) &~ U64_MASK;
        guestPtr = (threadHandle.jit->Regs()[Reg::SP] -= totalLen);
        Dynarmic_mem_1write(guestPtr, totalLen, (char *)hostPtr);
    }

    u32 guestPtr;
    size_t totalLen;
};

// wrapper to access guest string depending on its boundary
#define DynarmicHostString_NEED_FREE (1ull << 63)
class DynarmicHostString {
public:
    ~DynarmicHostString() {
        if(shouldFree) {
            if(dirty) {
                Dynarmic_mem_1write(guestPtr, totalLen, hostPtr);
            }
            free(hostPtr);
        }
    }

    // Initialize the wrapper with a given guest string pointer
    DynarmicHostString(u32 guestPtr, u32 len = 0) : dirty{false}, guestPtr{guestPtr} {
         char *dest = (char *)get_memory(guestPtr);
         if(!dest) {
             abort();
         }

         totalLen = len ?: strlen(dest);
         u32 pageOff = guestPtr & DYN_PAGE_MASK;
         shouldFree = pageOff + totalLen >= DYN_PAGE_SIZE;
         if(!shouldFree) {
             hostPtr = dest;
         } else {
             // If a length is not given, calculate until we hit null terminator
             if(!len) {
                 totalLen = DYN_PAGE_SIZE - pageOff; // avoid page overflow
                 for(u64 vaddr = (guestPtr - pageOff) + DYN_PAGE_SIZE;; vaddr += DYN_PAGE_SIZE) {
                     char *page = get_memory_page(vaddr);
                     if(!page) abort();
                     size_t len = strlen(page);
                     totalLen += len;
                     if(len < DYN_PAGE_SIZE) break;
                 }
             }
             hostPtr = (char *)malloc(totalLen + 1);
             hostPtr[totalLen] = '\0';
             Dynarmic_mem_1read(guestPtr, totalLen, hostPtr);
        }
    }

    // Detach hostPtr from being automatically freed by the destructor
    const char *hostPtrForGuest() {
        if(shouldFree) {
            // since we can't pass this object to guest, set a bit so we can detect it later
            hostPtr = (char *)((u64)hostPtr | DynarmicHostString_NEED_FREE);
        }
        shouldFree = false;
        return hostPtr;
    }

    char *hostPtrForWriting() {
        dirty = true;
        return hostPtr;
    }

    bool shouldFree, dirty;
    size_t totalLen;
    u32 guestPtr;
    char *hostPtr;
};
