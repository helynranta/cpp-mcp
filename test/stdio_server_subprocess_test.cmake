execute_process(
    COMMAND "${STDIO_SERVER_EXAMPLE}"
    INPUT_FILE "${REQUEST_FIXTURE}"
    OUTPUT_VARIABLE protocol_output
    ERROR_VARIABLE protocol_errors
    RESULT_VARIABLE process_result
    TIMEOUT 5
)

if(NOT process_result EQUAL 0)
    message(FATAL_ERROR "stdio server example failed (${process_result}): ${protocol_errors}")
endif()

string(REGEX MATCHALL "[^\n]+" protocol_messages "${protocol_output}")
list(LENGTH protocol_messages protocol_message_count)
if(NOT protocol_message_count EQUAL 3)
    message(FATAL_ERROR "expected 3 protocol responses, received ${protocol_message_count}: ${protocol_output}")
endif()

list(GET protocol_messages 0 initialize_response)
list(GET protocol_messages 1 list_response)
list(GET protocol_messages 2 call_response)

string(JSON initialized_protocol GET "${initialize_response}" result protocolVersion)
if(NOT initialized_protocol STREQUAL "2025-11-25")
    message(FATAL_ERROR "unexpected initialized protocol: ${initialize_response}")
endif()

string(JSON tool_name GET "${list_response}" result tools 0 name)
if(NOT tool_name STREQUAL "echo")
    message(FATAL_ERROR "echo tool missing from subprocess catalog: ${list_response}")
endif()

string(JSON echoed_text GET "${call_response}" result content 0 text)
if(NOT echoed_text STREQUAL "hello from subprocess")
    message(FATAL_ERROR "subprocess tool result mismatch: ${call_response}")
endif()
