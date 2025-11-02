/*
 * QEMU RISC-V G233 Board (Learning QEMU 2025)
 *
 * Copyright (c) 2025 Zevorn(Chao Liu) chao.liu@yeah.net
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_G233_H
#define HW_G233_H

#include "hw/boards.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/gpio/sifive_gpio.h"
#include "hw/riscv/g233_spi.h"

#define TYPE_RISCV_G233_SOC "riscv.gevico.g233.soc"
#define RISCV_G233_SOC(obj) \
    OBJECT_CHECK(G233SoCState, (obj), TYPE_RISCV_G233_SOC)

typedef struct G233SoCState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    RISCVHartArrayState cpus;
    DeviceState *plic;
    DeviceState *uart0;
    DeviceState *pwm0;
    SIFIVEGPIOState gpio;
    MemoryRegion mask_rom;

    /* SPI 设备改为指针，保持与其它 sysbus 设备一致 */
    DeviceState *spi;

} G233SoCState;

#define TYPE_RISCV_G233_MACHINE MACHINE_TYPE_NAME("g233")
#define RISCV_G233_MACHINE(obj) \
    OBJECT_CHECK(G233MachineState, (obj), TYPE_RISCV_G233_MACHINE)

typedef struct G233MachineState {
    /*< private >*/
    MachineState parent_obj;

    /*< public >*/
    G233SoCState soc;

} G233MachineState;

enum {
    G233_DEV_MROM,
    G233_DEV_CLINT,
    G233_DEV_PLIC,
    G233_DEV_GPIO0,
    G233_DEV_UART0, /* PL011 */
    G233_DEV_PWM0,
    G233_DEV_SPI,
    G233_DEV_DRAM
};

enum {
    G233_UART0_IRQ  = 1,
    G233_PWM0_IRQ   = 2,
    G233_SPI_IRQ    = 3,
    G233_GPIO0_IRQ0 = 8,
};

#define G233_PLIC_HART_CONFIG "M"
/*
 * Freedom E310 G002 and G003 supports 52 interrupt sources while
 * Freedom E310 G000 supports 51 interrupt sources. We use the value
 * of G002 and G003, so it is 53 (including interrupt source 0).
 */
#define G233_PLIC_NUM_SOURCES 53
#define G233_PLIC_NUM_PRIORITIES 7
#define G233_PLIC_PRIORITY_BASE 0x00
#define G233_PLIC_PENDING_BASE 0x1000
#define G233_PLIC_ENABLE_BASE 0x2000
#define G233_PLIC_ENABLE_STRIDE 0x80
#define G233_PLIC_CONTEXT_BASE 0x200000
#define G233_PLIC_CONTEXT_STRIDE 0x1000

#endif /* HW_G233_H */
