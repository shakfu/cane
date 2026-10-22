#include <print>

#include <libremidi/libremidi.hpp>

// Lists MIDI ports. Passes with zero ports, so it runs on headless machines.
int main(int, const char*[]) {
	libremidi::observer obs;

	for (const auto& port: obs.get_input_ports()) {
		std::println("in:  {}", port.display_name);
	}

	for (const auto& port: obs.get_output_ports()) {
		std::println("out: {}", port.display_name);
	}

	return 0;
}
