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

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "migration/vmstate.h"
#include "hw/riscv/g233_spi.h"


#define SPI_REG_CR1     0x00
#define SPI_REG_CR2     0x04
#define SPI_REG_SR      0x08
#define SPI_REG_DR      0x0C
#define SPI_REG_CSCTRL  0x10

/* 状态位定义 */
#define SR_RXNE (1 << 0)
#define SR_TXE  (1 << 1)
#define SR_OVERRUN (1 << 3)
#define SR_UNDERRUN (1 << 2)
#define SR_ERR  (SR_OVERRUN | SR_UNDERRUN)
#define SR_BSY  (1 << 7)

/* 控制位 */
#define CR1_SPE  (1 << 6)
#define CR1_MSTR (1 << 2)

#define CR2_RXNEIE (1 << 6)
#define CR2_TXEIE  (1 << 7)
#define CR2_ERRIE  (1 << 5)

static void g233_spi_update_irq(G233SPIState *s)
{
    bool txe_int  = (s->cr2 & CR2_TXEIE) && (s->sr & SR_TXE);
    bool rxne_int = (s->cr2 & CR2_RXNEIE) && (s->sr & SR_RXNE);
    bool err_int  = (s->cr2 & CR2_ERRIE) && (s->sr & SR_ERR);
    qemu_set_irq(s->irq, txe_int || rxne_int || err_int);
}

static void g233_spi_update_cs(G233SPIState *s)
{
    /* active 表示是否拉低片选信号 */
    for (int i = 0; i < 4; i++) {
        bool enabled = (s->cs >> i) & 1;        /* bit0..3: enable */
        bool active_low = (s->cs >> (i + 4)) & 1; /* bit4..7: active ? (1 means active) */
        int level = (enabled && active_low) ? 0 : 1;
        qemu_set_irq(s->cs_lines[i], level);
        printf("g233.spi: cs[%d] enabled=%d active_low=%d level=%d\n", i, enabled, active_low, level);
    }
}

/* SPI 数据传输：模拟一次 8bit 交换 */
static void g233_spi_transfer(G233SPIState *s)
{
    if(s->sr & SR_RXNE) {
        s->sr |= SR_OVERRUN;
        g233_spi_update_irq(s);
        return;
    }

    /* 实际传输：通过 QEMU ssi 总线接口 */
    s->sr &= ~SR_RXNE;
    s->sr &= ~SR_TXE;
    uint32_t out = s->dr;
    s->dr = ssi_transfer(s->ssi, s->dr);
    s->sr |= SR_RXNE | SR_TXE;    // 传输完成，RX、TX 

    printf("g233.spi: transfer out=0x%02x in=0x%02x\n", out, s->dr);

    g233_spi_update_irq(s);
}

static void g233_spi_reset(DeviceState *d)
{
    G233SPIState *s = G233_SPI(d);

    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = 0x00000002;
    s->dr = 0x0000000C;
    s->cs = 0x00000000;

    g233_spi_update_cs(s);
}

static uint64_t g233_spi_read(void *opaque, hwaddr offset, unsigned size)
{
    G233SPIState *s = opaque;
    uint32_t val = 0;

    switch (offset) {
    case SPI_REG_CR1: val = s->cr1; break;
    case SPI_REG_CR2: val = s->cr2; break;
    case SPI_REG_SR:  val = s->sr; break;
    case SPI_REG_DR:
        if ((s->sr & SR_RXNE) == 0) {
            s->sr |= SR_UNDERRUN;
        } else {
            s->sr &= ~SR_RXNE; // 读出数据后清 RXNE
        }
        val = s->dr;   
        g233_spi_update_irq(s);
        break;
    case SPI_REG_CSCTRL: val = s->cs; break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "g233.spi: invalid read offset 0x%"HWADDR_PRIx"\n", offset);
        break;
    }
    printf("g233.spi: read 0x%"PRIx32" from 0x%"HWADDR_PRIx"\n", val, offset);
    return val;
}

static void g233_spi_write(void *opaque, hwaddr offset, uint64_t val64, unsigned size)
{
    G233SPIState *s = opaque;

    uint32_t val = val64;
    printf("g233.spi: write 0x%"PRIx32" to 0x%"HWADDR_PRIx"\n", val, offset);

    switch (offset) {
    case SPI_REG_CR1:
        s->cr1 = val;
        break;

    case SPI_REG_CR2:
        s->cr2 = val;
        g233_spi_update_irq(s);
        break;

    case SPI_REG_SR:
        /* 清除错误标志 */
        if (val & SR_ERR) {
            s->sr &= ~SR_ERR;
        }
        g233_spi_update_irq(s);
        break;

    case SPI_REG_DR:
        s->dr = val & 0xFF;
        g233_spi_transfer(s);
        break;

    case SPI_REG_CSCTRL:
        s->cs = val & 0xFF;
        g233_spi_update_cs(s);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "g233.spi: invalid write offset 0x%"HWADDR_PRIx"\n", offset);
        break;
    }
}
static const MemoryRegionOps g233_spi_ops = {
    .read = g233_spi_read,
    .write = g233_spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void g233_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    G233SPIState *s = G233_SPI(dev);

    s->ssi = ssi_create_bus(dev, "spi");
    sysbus_init_irq(sbd, &s->irq);

    for (int i = 0; i < 4; i++) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &g233_spi_ops, s,
                          TYPE_G233_SPI, 0x1000);
    sysbus_init_mmio(sbd, &s->mmio);

    g233_spi_reset(dev);
}

static const VMStateDescription vmstate_g233_spi = {
    .name = TYPE_G233_SPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_U32(cr1, G233SPIState),
        VMSTATE_U32(cr2, G233SPIState),
        VMSTATE_U32(sr, G233SPIState),
        VMSTATE_U32(dr, G233SPIState),
        VMSTATE_U32(cs, G233SPIState),
        VMSTATE_END_OF_LIST()
    }
};

static void g233_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, g233_spi_reset);
    dc->realize = g233_spi_realize;
    dc->vmsd = &vmstate_g233_spi;
}

static const TypeInfo g233_spi_type_info = {
    .name           = TYPE_G233_SPI,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(G233SPIState),
    .class_init     = g233_spi_class_init,
};

static void g233_spi_register_types(void)
{
    type_register_static(&g233_spi_type_info);
}

type_init(g233_spi_register_types)