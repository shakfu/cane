#ifndef CANE_MIDI_HPP
#define CANE_MIDI_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <cane/value.hpp>

namespace cane {
	using MidiMessage = std::array<unsigned char, 3>;

	struct TimedMessage {
		TimeUnit at;
		MidiMessage bytes;
	};

	// One loop of a sequence as MIDI messages in time order.
	struct Schedule {
		std::vector<TimedMessage> messages;
		TimeUnit length { 0 };
	};

	// Events without a MIDI channel (1-16) or with a note above 127 are
	// skipped: they cannot be encoded as MIDI 1.0 messages.
	inline Schedule schedule(const Sequence& seq) {
		Schedule out;

		for (const auto& ev: seq) {
			out.length = std::max(out.length, ev.timestamp + ev.duration);

			if (ev.kind != EventKind::Beat or ev.channel < 1 or
				ev.channel > 16 or ev.note > 127) {
				continue;
			}

			auto channel = static_cast<unsigned char>(ev.channel - 1);
			auto velocity = static_cast<unsigned char>(ev.velocity & 0x7F);

			out.messages.push_back({ ev.timestamp,
				{ static_cast<unsigned char>(0x90 | channel), ev.note, velocity } });
			out.messages.push_back({ ev.timestamp + ev.duration,
				{ static_cast<unsigned char>(0x80 | channel), ev.note, 0 } });
		}

		// Note-offs before note-ons at the same time, so a repeated note is
		// released before it is struck again.
		std::ranges::stable_sort(out.messages, {}, [](const auto& m) {
			return std::pair { m.at, (m.bytes[0] & 0xF0) == 0x90 };
		});

		return out;
	}

	// Plays schedules on its own thread. `play` replaces the current one at the
	// end of its loop. Sounding notes are released on destruction.
	class Player {
		public:
		using Send = std::function<void(const MidiMessage&)>;

		private:
		Send send;

		std::mutex mutex;
		std::condition_variable_any cv;
		std::optional<std::pair<Schedule, bool>> next;  // Schedule, loop.
		bool idle = true;

		// Last, so it starts after the members it uses exist.
		std::jthread thread;

		using Clock = std::chrono::steady_clock;

		// Returns false if stopped first.
		bool sleep_until(std::stop_token stop, Clock::time_point when) {
			std::unique_lock lock { mutex };
			return not cv.wait_until(lock, stop, when, [] { return false; }) and
				not stop.stop_requested();
		}

		void run(std::stop_token stop) {
			Schedule current;
			bool looping = false;
			std::set<std::pair<unsigned char, unsigned char>> sounding;
			auto start = Clock::now();

			while (not stop.stop_requested()) {
				{
					std::unique_lock lock { mutex };

					// Idle until there is something to play.
					if (current.length == TimeUnit { 0 }) {
						idle = true;
						cv.notify_all();
						cv.wait(lock, stop, [&] { return next.has_value(); });
						start = Clock::now();
					}

					if (next.has_value()) {
						std::tie(current, looping) = std::move(next.value());
						next.reset();
						idle = false;
					}
				}

				if (current.length == TimeUnit { 0 }) {
					continue;
				}

				for (const auto& msg: current.messages) {
					if (not sleep_until(stop, start + msg.at)) {
						break;
					}

					send(msg.bytes);

					auto key = std::pair {
						static_cast<unsigned char>(msg.bytes[0] & 0x0F), msg.bytes[1]
					};

					if ((msg.bytes[0] & 0xF0) == 0x90) {
						sounding.insert(key);
					}

					else {
						sounding.erase(key);
					}
				}

				// The next loop starts from the scheduled time, not from now, so
				// timing does not drift.
				if (sleep_until(stop, start + current.length)) {
					start += current.length;
				}

				if (not looping) {
					current = {};
				}
			}

			for (auto [channel, note]: sounding) {
				send({ static_cast<unsigned char>(0x80 | channel), note, 0 });
			}
		}

		public:
		explicit Player(Send send_):
				send(std::move(send_)),
				thread([this](std::stop_token stop) { run(stop); }) {}

		Player(const Player&) = delete;
		Player& operator=(const Player&) = delete;

		// With `loop` false, plays one loop and then goes idle.
		void play(const Sequence& seq, bool loop = true) {
			std::scoped_lock lock { mutex };
			next = { schedule(seq), loop };
			cv.notify_all();
		}

		// Blocks until nothing is playing or queued.
		void wait_idle() {
			std::unique_lock lock { mutex };
			cv.wait(lock, [&] { return idle and not next.has_value(); });
		}
	};

	// The port whose name equals `name`, else the only one containing it.
	// On failure, returns a message naming the candidates.
	template <typename Port>
	inline std::pair<std::optional<Port>, std::string>
	find_port(const std::vector<Port>& ports, std::string_view name) {
		std::vector<const Port*> matches;

		for (const auto& port: ports) {
			if (port.display_name == name or port.port_name == name) {
				return { port, "" };
			}

			if (port.display_name.find(name) != std::string::npos) {
				matches.push_back(&port);
			}
		}

		if (matches.size() == 1) {
			return { *matches.front(), "" };
		}

		std::string error = matches.empty() ?
			std::format("no output port matches '{}'", name) :
			std::format("'{}' matches several output ports:", name);

		for (const auto* port: matches) {
			error += std::format("\n  {}", port->display_name);
		}

		return { std::nullopt, error };
	}
}  // namespace cane

#endif
