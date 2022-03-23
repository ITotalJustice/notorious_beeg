// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>

namespace gba::timer
{

struct Timer
{
	std::uint32_t event_time;
	std::uint16_t cycles;
	std::uint16_t counter; // timer, but timer.timer looks strange
	std::uint16_t reload;
	std::uint16_t freq;
	bool cascade;
	bool irq;
	bool enable;

};

auto on_timer0_event(Gba& gba) -> void;
auto on_timer1_event(Gba& gba) -> void;
auto on_timer2_event(Gba& gba) -> void;
auto on_timer3_event(Gba& gba) -> void;

auto on_savestate_load(Gba& gba) -> void;
auto on_cnt0_write(Gba& gba, u16 cnt) -> void;
auto on_cnt1_write(Gba& gba, u16 cnt) -> void;
auto on_cnt2_write(Gba& gba, u16 cnt) -> void;
auto on_cnt3_write(Gba& gba, u16 cnt) -> void;

auto update_timer(Gba& gba, Timer& timer) -> void;
auto read_timer(Gba& gba, std::uint8_t num) -> std::uint16_t;

auto run(Gba& gba, std::uint8_t cycles) -> void;

} // namespace gba::timer
