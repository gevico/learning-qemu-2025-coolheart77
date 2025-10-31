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

#ifndef HW_RISCV_G233_SPI_H
#define HW_RISCV_G233_SPI_H

#include "qemu/fifo8.h"
#include "hw/sysbus.h"

#define TYPE_SIFIVE_SPI "g233.spi"

typedef struct G233SPIState {
    SysBusDevice parent_obj;

    uint32_t cr1;
    uint32_t cr2;
    
} G233SPIState;

#endif /* HW_RISCV_G233_SPI_H */