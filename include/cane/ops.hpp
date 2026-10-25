#ifndef CANE_OPS_HPP
#define CANE_OPS_HPP

#include <cstddef>
#include <functional>
#include <numeric>
#include <algorithm>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <ranges>

#include <cane/log.hpp>
#include <cane/value.hpp>

namespace cane {

	namespace detail {
		inline EventKind bool_to_event(bool b) {
			std::array v {
				EventKind::Rest,
				EventKind::Beat,
			};

			return v.at(b);
		}

		inline EventKind bit_not_event(EventKind e) {
			return bool_to_event(not static_cast<bool>(e));
		}

		inline EventKind bit_and_event(EventKind lhs, EventKind rhs) {
			return bool_to_event(
				static_cast<bool>(lhs) and static_cast<bool>(rhs)
			);
		}

		inline EventKind bit_or_event(EventKind lhs, EventKind rhs) {
			return bool_to_event(
				static_cast<bool>(lhs) or static_cast<bool>(rhs)
			);
		}

		inline EventKind bit_xor_event(EventKind lhs, EventKind rhs) {
			return bool_to_event(
				static_cast<bool>(lhs) xor static_cast<bool>(rhs)
			);
		}
	}  // namespace detail

	// Misc.
	template <typename T>
	inline size_t beats(const T& v) {
		return std::ranges::count_if(
			v, std::bind_front(std::equal_to {}, EventKind::Beat)
		);
	}

	template <typename T>
	inline size_t rests(const T& v) {
		return std::ranges::count_if(
			v, std::bind_front(std::equal_to {}, EventKind::Rest)
		);
	}

	template <typename T>
	inline T euclidean(size_t beats, size_t steps) {
		return std::views::iota(0) | std::views::take(steps) |
			std::views::transform([&](const auto& i) {
				   return detail::bool_to_event(((i * beats) % steps) < beats);
			   }) |
			std::ranges::to<T>();
	}

	// Identifies repeating pattern in a sequence
	// and attempts to minify it so we don't spam
	// the stdout for large sequences.
	template <typename T>
	inline T minify(T seq) {
		for (size_t f = 1; f != seq.size(); ++f) {
			if (seq.size() % f != 0) {
				continue;
			}

			bool all_eq = false;
			for (size_t i = 1; i != (seq.size() / f); ++i) {
				all_eq = std::equal(
					seq.begin(), seq.begin() + f, seq.begin() + (f * i)
				);

				if (not all_eq) {
					break;
				}
			}

			if (all_eq) {
				seq.erase(seq.begin() + f, seq.end());
				return seq;
			}
		}

		return seq;
	}

	// Accessors
	template <typename T>
	inline T head(T v, size_t n = 1) {
		return v | std::views::take(n) | std::ranges::to<T>();
	}

	template <typename T>
	inline T tail(T v, size_t n = 1) {
		return v | std::views::drop(n) | std::ranges::to<T>();
	}

	// Transformations
	template <typename T>
	inline T repeat(T v, size_t n) {
		// Copy sequence N times to the end of itself.
		// turns i.e. `[a b c]` where n = 3 into `[a b c a b c a b c]`.

		return std::views::repeat(v, n) | std::views::join |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T reverse(T v) {
		return v | std::views::reverse | std::ranges::to<T>();
	}

	template <typename T>
	inline T rotate_left(T v, size_t n) {
		std::ranges::rotate(v, v.begin() + (n % v.size()));
		return v;
	}

	template <typename T>
	inline T rotate_right(T v, size_t n) {
		auto seq = v | std::views::reverse;
		std::ranges::rotate(seq, seq.begin() + (n % seq.size()));
		return v;
	}

	template <typename T>
	inline T concatenate(T lhs, T rhs) {
		// TODO: C++26 std::views::concat
		lhs.insert(lhs.end(), rhs.begin(), rhs.end());
		return lhs;
	}

	// Logic
	template <typename T>
	inline T bit_not(T v) {
		return v | std::views::transform(detail::bit_not_event) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T bit_and(T lhs, T rhs) {
		std::ranges::transform(lhs, rhs, lhs.begin(), detail::bit_and_event);
		return lhs;
	}

	template <typename T>
	inline T bit_or(T lhs, T rhs) {
		std::ranges::transform(lhs, rhs, lhs.begin(), detail::bit_or_event);
		return lhs;
	}

	template <typename T>
	inline T bit_xor(T lhs, T rhs) {
		std::ranges::transform(lhs, rhs, lhs.begin(), detail::bit_xor_event);
		return lhs;
	}

	// Arithmetic
	template <typename T>
	inline T vector_add(T lhs, T rhs) {
		return std::views::zip_transform(std::plus {}, lhs, rhs) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T vector_sub(T lhs, T rhs) {
		return std::views::zip_transform(std::minus {}, lhs, rhs) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T vector_mul(T lhs, T rhs) {
		return std::views::zip_transform(std::multiplies {}, lhs, rhs) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T vector_div(T lhs, T rhs) {
		return std::views::zip_transform(std::divides {}, lhs, rhs) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T scalar_add(T v, size_t scalar) {
		return v |
			std::views::transform(std::bind_front(std::plus {}, scalar)) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T scalar_sub(T v, size_t scalar) {
		return v |
			std::views::transform(std::bind_back(std::minus {}, scalar)) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T scalar_mul(T v, size_t scalar) {
		return v |
			std::views::transform(std::bind_front(std::multiplies {}, scalar)) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline T scalar_div(T v, size_t scalar) {
		return v |
			std::views::transform(std::bind_back(std::divides {}, scalar)) |
			std::ranges::to<T>();
	}

	template <typename T>
	inline int64_t random(int64_t min, int64_t max) {
		// TODO: Handle negative numbers

		std::random_device rd;
		std::mt19937 rng { rd() };

		std::uniform_int_distribution dist(min, max);

		return dist(rng);
	}

	// Raw engine bits, not a distribution: `std::mt19937_64` output is fixed
	// by the standard, distributions are not, so picks match across platforms.
	template <typename T>
	inline T choice(T lhs, T rhs, std::mt19937_64& rng) {
		return (rng() & 1) ? lhs : rhs;
	}

	// Scaling only durations left start times unchanged, so events overlapped
	// or left gaps instead of the sequence slowing down or speeding up.
	template <typename T>
	inline T timemul(T v, size_t scalar) {
		for (auto& ev: v) {
			ev.timestamp *= scalar;
			ev.duration *= scalar;
		}

		return v;
	}

	template <typename T>
	inline T timediv(T v, size_t scalar) {
		for (auto& ev: v) {
			ev.timestamp /= scalar;
			ev.duration /= scalar;
		}

		return v;
	}

	// Sequences

	// End of the last event: the length of one loop.
	template <typename T>
	inline TimeUnit length(const T& seq) {
		TimeUnit end { 0 };

		for (const auto& ev: seq) {
			end = std::max(end, ev.timestamp + ev.duration);
		}

		return end;
	}

	template <typename T>
	inline T sort_by_time(T seq) {
		std::ranges::stable_sort(seq, {}, &Event::timestamp);
		return seq;
	}

	// `rhs` starts where `lhs` ends. Offsetting by the last start time instead
	// started `rhs` on `lhs`'s final step.
	template <typename T>
	inline T join(T lhs, T rhs) {
		auto offset = length(lhs);

		for (auto ev: rhs) {
			ev.timestamp += offset;
			lhs.push_back(ev);
		}

		return lhs;
	}

	// Repeats each layer to the LCM of their lengths, so a layer whose length
	// does not divide the other's keeps its own period (polymeter).
	template <typename T>
	inline T layer(T lhs, T rhs, TimeUnit step, Scalar max_steps) {
		auto a = length(lhs).count();
		auto b = length(rhs).count();

		// `lcm(x, 0)` is 0, and repeating by `0 / x` would drop the layer.
		if (a == 0 or b == 0) {
			return sort_by_time(concatenate(std::move(lhs), std::move(rhs)));
		}

		// Checked before multiplying, so the LCM cannot overflow.
		auto g = std::gcd(a, b);
		auto limit = max_steps * step.count();

		if (a / g > limit / b) {
			auto steps = [&](auto us) {
				return static_cast<double>(us) / static_cast<double>(step.count());
			};

			cane::report(
				ReportKind::Eval,
				"layers of {:g} and {:g} steps loop every {:g} steps, over the "
				"limit of {}",
				steps(a),
				steps(b),
				steps(a / g) * static_cast<double>(b),
				max_steps
			);
		}

		auto lcm = a / g * b;

		return sort_by_time(concatenate(
			repeat_sequence(std::move(lhs), lcm / a),
			repeat_sequence(std::move(rhs), lcm / b)
		));
	}

	// Rests keep their note: they are never sent.
	template <typename T>
	inline T transpose(T seq, Scalar n) {
		for (auto& ev: seq) {
			if (ev.kind == EventKind::Rest) {
				continue;
			}

			auto note = ev.note + n;

			if (note < 0 or note > 127) {
				cane::report(
					ReportKind::Eval,
					"note {} transposed by {} is outside 0-127",
					ev.note,
					n
				);
			}

			ev.note = static_cast<uint8_t>(note);
		}

		return seq;
	}

	// Assigns `values` to events in order, cycling, as `@` assigns notes.
	// Rests use up a value too.
	template <typename T, typename F>
	inline T assign_each(
		T seq, const Melody& values, std::string_view op, Scalar lo, Scalar hi, F set
	) {
		if (values.empty()) {
			cane::report(ReportKind::Eval, "`{}` needs at least one value", op);
		}

		for (auto [i, ev]: std::views::zip(std::views::iota(size_t { 0 }), seq)) {
			auto v = values[i % values.size()];

			if (v < lo or v > hi) {
				cane::report(
					ReportKind::Eval, "`{} {}` is outside {}-{}", op, v, lo, hi
				);
			}

			set(ev, v);
		}

		return seq;
	}

	// Names follow Tidal's `scale` and `chord`, so patterns copy across.
	struct NamedSteps {
		std::string_view name;
		std::vector<Scalar> steps;
	};

	inline const std::vector<NamedSteps>& scales() {
		static const std::vector<NamedSteps> table {
			{ "major", { 0, 2, 4, 5, 7, 9, 11 } },
			{ "minor", { 0, 2, 3, 5, 7, 8, 10 } },
			{ "dorian", { 0, 2, 3, 5, 7, 9, 10 } },
			{ "phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
			{ "lydian", { 0, 2, 4, 6, 7, 9, 11 } },
			{ "mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
			{ "locrian", { 0, 1, 3, 5, 6, 8, 10 } },
			{ "harmonicMinor", { 0, 2, 3, 5, 7, 8, 11 } },
			{ "melodicMinor", { 0, 2, 3, 5, 7, 9, 11 } },
			{ "majPent", { 0, 2, 4, 7, 9 } },
			{ "minPent", { 0, 3, 5, 7, 10 } },
			{ "whole", { 0, 2, 4, 6, 8, 10 } },
			{ "chromatic", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
		};

		return table;
	}

	inline const std::vector<NamedSteps>& chords() {
		static const std::vector<NamedSteps> table {
			{ "maj", { 0, 4, 7 } },
			{ "min", { 0, 3, 7 } },
			{ "dim", { 0, 3, 6 } },
			{ "aug", { 0, 4, 8 } },
			{ "sus2", { 0, 2, 7 } },
			{ "sus4", { 0, 5, 7 } },
			{ "maj7", { 0, 4, 7, 11 } },
			{ "dom7", { 0, 4, 7, 10 } },
			{ "min7", { 0, 3, 7, 10 } },
		};

		return table;
	}

	// The error lists every name, so a typo shows the alternatives.
	inline const std::vector<Scalar>& lookup_steps(
		const std::vector<NamedSteps>& table, std::string_view kind, std::string_view name
	) {
		auto it = std::ranges::find(table, name, &NamedSteps::name);

		if (it == table.end()) {
			std::string names;

			for (const auto& entry: table) {
				names += (names.empty() ? "" : ", ") + std::string { entry.name };
			}

			cane::report(
				ReportKind::Eval, "unknown {} `{}`; one of: {}", kind, name, names
			);
		}

		return it->steps;
	}

	// Degree `d` of `steps`, in semitones. Degrees past either end wrap by an
	// octave: in a 7-note scale, 7 is 12 and -1 is -1.
	inline Scalar scale_degree(const std::vector<Scalar>& steps, Scalar d) {
		auto len = static_cast<Scalar>(steps.size());
		auto octave = (d >= 0 ? d : d - len + 1) / len;  // Floor division.

		return 12 * octave + steps[static_cast<size_t>(d - octave * len)];
	}

	// One copy of `seq` per interval, all starting together: a block chord.
	template <typename T>
	inline T stack(const T& seq, const Melody& intervals) {
		if (intervals.empty()) {
			cane::report(ReportKind::Eval, "`stack` needs at least one interval");
		}

		T out;

		for (auto i: intervals) {
			out = concatenate(std::move(out), transpose(seq, i));
		}

		return sort_by_time(std::move(out));
	}

	template <typename T>
	inline T repeat_sequence(T seq, Scalar n) {
		if (n < 0) {
			cane::report(ReportKind::Eval, "cannot repeat {} times", n);
		}

		auto len = length(seq);
		T out;

		for (Scalar i = 0; i < n; ++i) {
			for (auto ev: seq) {
				ev.timestamp += len * i;
				out.push_back(ev);
			}
		}

		return out;
	}

	// Shifts every event later by `shift` (earlier if negative), wrapping at
	// the loop length.
	template <typename T>
	inline T rotate_sequence(T seq, TimeUnit shift) {
		auto len = length(seq);

		if (len == TimeUnit { 0 }) {
			return seq;
		}

		for (auto& ev: seq) {
			ev.timestamp = ((ev.timestamp + shift) % len + len) % len;
		}

		return sort_by_time(std::move(seq));
	}

	// Rescales `seq` to last exactly `target`. Start and end times are scaled
	// separately from the originals, so rounding never accumulates and the
	// last end is exact: layering by LCM needs whole-step lengths.
	template <typename T>
	inline T fit(T seq, TimeUnit target) {
		auto len = length(seq).count();

		if (len == 0) {
			return seq;
		}

		auto scale = [&](TimeUnit t) {
			return TimeUnit { t.count() * target.count() / len };
		};

		for (auto& ev: seq) {
			auto end = scale(ev.timestamp + ev.duration);
			ev.timestamp = scale(ev.timestamp);
			ev.duration = end - ev.timestamp;
		}

		return seq;
	}

	template <typename T>
	inline T reverse_sequence(T seq) {
		auto len = length(seq);

		for (auto& ev: seq) {
			ev.timestamp = len - (ev.timestamp + ev.duration);
		}

		return sort_by_time(std::move(seq));
	}

	template <typename T>
	inline T cycle(T v, size_t length) {
		// Cycle elements of `v` such that the length of the container matches
		// `length`.

		return std::views::repeat(v) | std::views::join |
			std::views::take(length) | std::ranges::to<T>();
	}

	template <typename S, typename R, typename M>
	inline S map(TimeUnit duration, R rhythm, M melody) {
		size_t length = std::max(rhythm.size(), melody.size());

		rhythm = cycle(rhythm, length);
		melody = cycle(melody, length);

		S seq;
		TimeUnit timestamp { 0 };

		for (auto [key, step, note]:
			 std::views::zip(std::views::iota(0), rhythm, melody)) {
			seq.emplace_back(step, timestamp, duration, 0, note, 127);

			timestamp += duration;
		}

		return seq;
	}

	template <typename T, typename D>
	inline T map_duration(T v, D duration) {
		for (auto& event: v) {
			event.timestamp = duration;
			event.duration = duration;
		}

		return v;
	}

}  // namespace cane

#endif
