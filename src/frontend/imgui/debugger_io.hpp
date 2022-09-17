// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

namespace gba { struct Gba; }

namespace debugger::io {

auto render_gb(gba::Gba& gba, bool* p_open) -> void;
auto render_gba(gba::Gba& gba, bool* p_open) -> void;

} // namespace debugger::io
