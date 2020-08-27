#pragma once

#include "mn/Exports.h"
#include "mn/IO.h"
#include "mn/Map.h"

#include <stdint.h>

namespace mn
{
	union UUID
	{
		struct
		{
			uint32_t time_low;
			uint16_t time_mid;
			uint16_t time_hi_and_version;
			uint8_t clk_seq_hi_res;
			uint8_t clk_seq_low;
			uint8_t node[6];
		} parts;
		uint64_t rnd[2];
		uint8_t bytes[16];
	};

	inline static bool
	operator==(const UUID &a, const UUID &b)
	{
		return (a.rnd[0] == b.rnd[0]) && (a.rnd[1] == b.rnd[1]);
	}

	inline static bool
	operator!=(const UUID &a, const UUID &b)
	{
		return !operator==(a, b);
	}

	MN_EXPORT UUID
	uuid_generate();

	template<>
	struct Hash<UUID>
	{
		size_t
		operator()(const UUID &v) const
		{
			return murmur_hash(Block{(void *)v.bytes, size_t(sizeof(v.bytes))});
		}
	};
} // namespace mn

namespace fmt
{
	template<>
	struct formatter<mn::UUID>
	{
		template<typename ParseContext>
		constexpr auto
		parse(ParseContext &ctx)
		{
			return ctx.begin();
		}

		template<typename FormatContext>
		auto
		format(const mn::UUID &self, FormatContext &ctx)
		{
			return format_to(
				ctx.out(),
				"{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
				self.parts.time_low,
				self.parts.time_mid,
				self.parts.time_hi_and_version,
				self.parts.clk_seq_hi_res,
				self.parts.clk_seq_low,
				self.parts.node[0],
				self.parts.node[1],
				self.parts.node[2],
				self.parts.node[3],
				self.parts.node[4],
				self.parts.node[5]);
		}
	};
} // namespace fmt