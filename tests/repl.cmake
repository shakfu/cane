# Pipes tests/repl.txt into the repl. Checks that errors do not end the
# session, bindings persist across lines, and a failed line commits none.
# Usage: cmake -DCANE=<path> -DINPUT=<path> -P repl.cmake
execute_process(
	COMMAND ${CANE} --no-midi
	INPUT_FILE ${INPUT}
	RESULT_VARIABLE result
	OUTPUT_VARIABLE out
	ERROR_VARIABLE err
)

string(APPEND err "${out}")

if(NOT result EQUAL 0)
	message(FATAL_ERROR "repl exited with ${result}:\n${err}")
endif()

foreach(expected "syntax error" "unknown binding `q`" "unknown binding `w`")
	string(FIND "${err}" "${expected}" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "missing `${expected}`:\n${err}")
	endif()
endforeach()

foreach(name x y z t)
	string(FIND "${err}" "unknown binding `${name}`" at)
	if(NOT at EQUAL -1)
		message(FATAL_ERROR "`${name}` did not persist:\n${err}")
	endif()
endforeach()
