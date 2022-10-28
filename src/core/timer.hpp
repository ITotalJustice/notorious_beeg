// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::timer {

struct Timer
{
	s32 event_time;
	u16 cycles;
	u16 counter; // timer, but timer.timer looks strange
	u16 reload;
	u16 freq;
	bool cascade;
	bool irq;
	bool enable;
};

auto on_timer_event(void* user, s32 id, s32 late) -> void;
auto on_cnt_write(Gba& gba, u8 num) -> void;
auto read_timer(Gba& gba, u8 num) -> u16;
void write_timer(Gba& gba, u16 value, u8 num);

} // namespace gba::timer
