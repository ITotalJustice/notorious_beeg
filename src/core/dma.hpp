// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>

namespace gba::dma
{
struct Channel
{
	enum class Mode : std::uint8_t
	{
		immediate = 0b00,
		vblank = 0b01,
		hblank = 0b10,
		special = 0b11,
	};

	enum class IncrementType : std::uint8_t
	{
		inc = 0b00,
		dec = 0b01,
		unchanged = 0b10,
		// src: invalid
		// dst: reload dst address
		special = 0b11,
	};

	enum class SizeType : bool
	{
		half = 0, // 16bit
		word = 1, // 32bit
	};

	std::uint32_t len;
	std::uint32_t src_addr;
	std::uint32_t dst_addr;

	std::int8_t src_increment;
	std::int8_t dst_increment;

	Mode mode;
	IncrementType src_increment_type;
	IncrementType dst_increment_type;
	SizeType size_type;

	bool repeat;
	bool irq;
	bool enabled;
};

// uint32_t& dmasad, uint32_t& dmadad, uint16_t& dmacnt_h, uint16_t& dmacnt_l
auto on_event(Gba& gba) -> void;
auto on_hblank(Gba& gba) -> void;
auto on_vblank(Gba& gba) -> void;
auto on_fifo_empty(Gba& gba, std::uint8_t num) -> void;
auto on_cnt_write(Gba& gba, std::uint8_t channel_num) -> void;
} // namespace gba::dma
