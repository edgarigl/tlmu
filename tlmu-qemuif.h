enum tlmu_boot_state {
    TLMU_BOOT_SLEEPING,
    TLMU_BOOT_RUNNING
};

enum tlmu_event {
    TLMU_TLM_EVENT_NONE,
    TLMU_TLM_EVENT_SYNC,
    TLMU_TLM_EVENT_WAKE,
    TLMU_TLM_EVENT_SLEEP,
    TLMU_TLM_EVENT_IRQ,
    TLMU_TLM_EVENT_INVALIDATE_DMI,
    TLMU_TLM_EVENT_RESET,
    TLMU_TLM_EVENT_DEBUG_BREAK,
};


enum {
    TLMU_DMI_PROT_NONE = 0,
    TLMU_DMI_PROT_FAST = 1,
    TLMU_DMI_PROT_READ = 2,
    TLMU_DMI_PROT_WRITE = 4,
    TLMU_DMI_PROT_BROKEN = 16,
};

struct tlmu_irq
{
    uint64_t addr;
    uint32_t data;
};

struct tlmu_dmi
{
    void *ptr;                   /* Host pointer for direct access.  */
    uint64_t base;               /* Physical address represented by *ptr.  */
    uint64_t size;               /* Size of DMI mapping.  */
    int prot;                    /* Protection bits.  */
    unsigned int read_latency;   /* Read access delay.  */
    unsigned int write_latency;  /* Write access delay.  */
};
