#include <cstdlib>

#include <cane/log.hpp>

// CANE_DIE must throw. Returning success means it did not.
int main(int, const char*[]) {
	try {
		CANE_DIE("die");
	}

	catch (const cane::Fatal&) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
