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

#include "g233_spi.h"
#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"

static const TypeInfo g233_spi_type_info = {
    .name           = TYPE_SIFIVE_SPI,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(G233SPIState),
    //.class_init     = sifive_spi_class_init,
};

static void g233_spi_register_types(void)
{
    type_register_static(&g233_spi_type_info);
}

type_init(g233_spi_register_types)