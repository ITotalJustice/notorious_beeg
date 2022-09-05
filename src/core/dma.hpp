// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::dma {

enum class Mode : u8
{
	immediate = 0b00,
	vblank = 0b01,
	hblank = 0b10,
	special = 0b11,
};

enum class IncrementType : u8
{
	inc = 0b00,
	dec = 0b01,
	unchanged = 0b10,
	// src: invalid
	// dst: reload dst address
	special = 0b11,
};

enum class SizeType : u8
{
	half = 0, // 16bit
	word = 1, // 32bit
};

struct Channel
{
	u32 len;
	u32 src_addr;
	u32 dst_addr;

	s8 src_increment;
	s8 dst_increment;

	Mode mode;
	IncrementType src_increment_type;
	IncrementType dst_increment_type;
	SizeType size_type;

	bool repeat;
	bool irq;
	bool enabled;
};

STATIC auto on_event(Gba& gba) -> void;
STATIC auto on_hblank(Gba& gba) -> void;
STATIC auto on_vblank(Gba& gba) -> void;
STATIC auto on_fifo_empty(Gba& gba, u8 num) -> void;
STATIC auto on_cnt_write(Gba& gba, u8 channel_num) -> void;

} // namespace gba::dma
