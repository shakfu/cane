#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <cane/cane.hpp>
#include <cane/midi.hpp>

using namespace std::chrono_literals;

namespace {
	int failures = 0;

	void check(bool ok, std::string_view what) {
		if (not ok) {
			std::println(stderr, "FAIL: {}", what);
			++failures;
		}
	}

	// 6000 BPM: one step every 10 ms.
	cane::Sequence pattern(std::string_view src) {
		cane::Session session { { .bpm = 6000, .channel_bindings = {
			{ "drums", 10 },
		}, } };

		return std::get<cane::Sequence>(session.eval(std::string { src }).value);
	}

	// Records what a Player sends.
	struct Recorder {
		std::mutex mutex;
		std::vector<cane::MidiMessage> sent;

		cane::Player::Send sender() {
			return [this](const cane::MidiMessage& msg) {
				std::scoped_lock lock { mutex };
				sent.push_back(msg);
			};
		}

		std::vector<cane::MidiMessage> get() {
			std::scoped_lock lock { mutex };
			return sent;
		}
	};

	size_t count(const std::vector<cane::MidiMessage>& sent, cane::MidiMessage msg) {
		return static_cast<size_t>(std::ranges::count(sent, msg));
	}

	struct Port {
		std::string display_name;
		std::string port_name;
	};
}  // namespace

int main(int, const char*[]) {
	try {
		// schedule: note-on/off per beat on channel 10 (status 0x99/0x89),
		// rests skipped, note-off sorted before a note-on at the same time.
		auto sched = cane::schedule(pattern("36@!.! ~> \"drums\""));
		check(sched.length == 30ms, "schedule length");
		check(sched.messages.size() == 4, "schedule size");
		check(
			sched.messages.size() == 4 and
				sched.messages[0].bytes == cane::MidiMessage { 0x99, 36, 127 } and
				sched.messages[1].bytes == cane::MidiMessage { 0x89, 36, 0 } and
				sched.messages[1].at == 10ms,
			"schedule messages"
		);

		auto repeat = cane::schedule(pattern("36@!! ~> \"drums\""));
		check(
			repeat.messages.size() == 4 and repeat.messages[1].at == 10ms and
				(repeat.messages[1].bytes[0] & 0xF0) == 0x80 and
				(repeat.messages[2].bytes[0] & 0xF0) == 0x90,
			"note-off before note-on"
		);

		// Velocity goes in the note-on. The gate moves the note-off but not
		// the loop length, and a note-off never passes the loop end.
		auto gated = cane::schedule(pattern("36@!. vel 90 gate 50 ~> \"drums\""));
		check(
			gated.length == 20ms and gated.messages.size() == 2 and
				gated.messages[0].bytes == cane::MidiMessage { 0x99, 36, 90 } and
				gated.messages[1].at == 5ms,
			"schedule gate and vel"
		);

		auto held = cane::schedule(pattern("36@.! gate 400 ~> \"drums\""));
		check(
			held.length == 20ms and held.messages.size() == 2 and
				held.messages[1].at == 20ms,
			"schedule clamps note-off"
		);

		// No channel (never sent with ~>): not encodable, so skipped.
		cane::Session session { { .bpm = 6000 } };
		auto unsent = std::get<cane::Sequence>(session.eval("36@!!").value);
		check(cane::schedule(unsent).messages.empty(), "no channel skipped");

		// Player: loops, switches at the loop end, and releases notes on stop.
		Recorder rec;
		{
			cane::Player player { rec.sender() };
			player.play(pattern("36@!. ~> \"drums\""));
			std::this_thread::sleep_for(75ms);  // About 3 loops of 20 ms.

			auto first = rec.get();
			check(count(first, { 0x99, 36, 127 }) >= 2, "player loops");

			player.play(pattern("38@!. ~> \"drums\""));
			std::this_thread::sleep_for(75ms);
			check(count(rec.get(), { 0x99, 38, 127 }) >= 2, "player switches");
		}

		auto sent = rec.get();
		check(
			count(sent, { 0x99, 36, 127 }) == count(sent, { 0x89, 36, 0 }) and
				count(sent, { 0x99, 38, 127 }) == count(sent, { 0x89, 38, 0 }),
			"every note released"
		);

		// Play once: one loop, then idle.
		Recorder once;
		{
			cane::Player player { once.sender() };
			player.play(pattern("36@!.! ~> \"drums\""), false);
			player.wait_idle();
			std::this_thread::sleep_for(50ms);
		}

		check(count(once.get(), { 0x99, 36, 127 }) == 2, "play once");

		// Player: one cycle per loop, numbered from 0, evaluated ahead.
		{
			std::mutex mutex;
			std::vector<cane::Scalar> asked;
			Recorder cyc;
			{
				cane::Player player { cyc.sender() };
				player.play([&](cane::Scalar n) {
					std::scoped_lock lock { mutex };
					asked.push_back(n);
					return pattern(n % 2 == 0 ? "36@!. ~> \"drums\"" : "38@!. ~> \"drums\"");
				});
				std::this_thread::sleep_for(75ms);
			}

			std::vector<unsigned char> ons;

			for (const auto& msg: cyc.get()) {
				if ((msg[0] & 0xF0) == 0x90) {
					ons.push_back(msg[1]);
				}
			}

			std::scoped_lock lock { mutex };
			check(
				asked.size() >= 3 and asked[0] == 0 and asked[1] == 1 and asked[2] == 2,
				"player numbers cycles"
			);
			check(
				ons.size() >= 2 and ons[0] == 36 and ons[1] == 38,
				"player plays each cycle"
			);
		}

		// A cycle that fails to evaluate repeats the one before.
		{
			Recorder err;
			{
				cane::Player player { err.sender() };
				player.play([](cane::Scalar n) {
					if (n == 1) {
						throw cane::Fatal { "cycle 1 failed\n" };
					}

					return pattern(n == 0 ? "36@!. ~> \"drums\"" : "38@!. ~> \"drums\"");
				});
				std::this_thread::sleep_for(75ms);
			}

			std::vector<unsigned char> ons;

			for (const auto& msg: err.get()) {
				if ((msg[0] & 0xF0) == 0x90) {
					ons.push_back(msg[1]);
				}
			}

			check(
				ons.size() >= 3 and ons[0] == 36 and ons[1] == 36 and ons[2] == 38,
				"player repeats on error"
			);
		}

		// find_port: exact name first, then a unique substring.
		std::vector<Port> ports { { "IAC Bus 1", "a" }, { "IAC Bus 10", "b" } };
		check(cane::find_port(ports, "IAC Bus 1").first->port_name == "a", "exact");
		check(cane::find_port(ports, "10").first->port_name == "b", "substring");
		check(not cane::find_port(ports, "IAC").first, "ambiguous");
		check(not cane::find_port(ports, "Synth").first, "no match");
	}

	catch (const cane::Fatal& e) {
		std::println(stderr, "FAIL: {}", e.what());
		return EXIT_FAILURE;
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
