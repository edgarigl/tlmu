/*
 * Map a TLMu area.
 *
 * env             - CPUState for the connected core.
 * addr            - Base address
 * size            - Size of mapping
 * sync_period_ns  - Sync timer interval
 * cpu_irq         - Interrupt lines
 * nr_irq          - Number of interrupt lines
 */
static inline void tlm_map(CPUState *env, uint64_t addr, uint64_t size,
                           uint64_t sync_period_ns,
                           qemu_irq *cpu_irq, uint32_t nr_irq)
{
    int i;

    DeviceState *dev;
    dev = qdev_create(NULL, "tlm,memory");
    qdev_prop_set_ptr(dev, "cpu_env", env);
    qdev_prop_set_uint64(dev, "base_addr", addr);
    qdev_prop_set_uint64(dev, "size", size);
    qdev_prop_set_uint64(dev, "sync_period_ns", sync_period_ns);
    qdev_prop_set_uint32(dev, "nr_irq", nr_irq);
    qdev_init_nofail(dev);
    sysbus_mmio_map(sysbus_from_qdev(dev), 0, addr);
    for (i = 0; i < nr_irq; i++) {
        sysbus_connect_irq(sysbus_from_qdev(dev), i, cpu_irq[i]);
    }
}

