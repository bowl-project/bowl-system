#include "system.h"

static BowlFunctionEntry system_functions[] = {
    { .name = "system:exit", .documentation = "", .function = system_exit },
    { .name = "system:chdir", .documentation = "", .function = system_change_directory },
    { .name = "system:dir", .documentation = "", .function = system_directory }
};

BowlValue bowl_module_initialize(BowlStack stack, BowlValue library) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    return bowl_register_all(&frame, frame.registers[0], system_functions, sizeof(system_functions) / sizeof(system_functions[0]));
}

BowlValue bowl_module_finalize(BowlStack stack, BowlValue library) {
    return NULL;
}

BowlValue system_exit(BowlStack stack) {
    BowlValue value; 

    BOWL_STACK_POP_VALUE(stack, &value);
    BOWL_ASSERT_TYPE(value, BowlNumberValue);

    exit((int) value->number.value);
    return NULL;
}

BowlValue system_change_directory(BowlStack stack) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);

    BOWL_STACK_POP_VALUE(&frame, &frame.registers[0]);
    BOWL_ASSERT_TYPE(frame.registers[0], BowlStringValue);

    char *path = unicode_to_string(&frame.registers[0]->string.codepoints[0], frame.registers[0]->string.length);
    if (path == NULL) {
        return bowl_exception_out_of_heap;
    }

    #if defined(OS_UNIX)
        if (chdir(path) == -1) {
            BowlValue result = bowl_format_exception(&frame, "failed to change the working directory to '%s'", path).value;
            free(path);
            return result;
        }
    #elif defined(OS_WINDOWS)
        if(_chdir(path) == -1) {
            BowlValue result = bowl_format_exception(&frame, "failed to change the working directory to '%s'", path).value;
            free(path);
            return result;
        }
    #endif

    free(path);
    return NULL;
}

BowlValue system_directory(BowlStack stack) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);

    u64 capacity = 4096;
    char *buffer = malloc(capacity * sizeof(char));

    if (buffer == NULL) {
        return bowl_exception_out_of_heap;
    }

    do {
        if (getcwd(buffer, capacity) == NULL) {
            if (errno == ERANGE) {
                capacity *= 2;
                char *const temporary = realloc(buffer, capacity);
                if (temporary == NULL) {
                    free(buffer);
                    return bowl_exception_out_of_heap;
                }
                buffer = temporary;
            } else {
                free(buffer);
                return bowl_format_exception(&frame, "failed to read the current working directory").value;
            }
        } else {
            break;
        }
    } while (true);

    BOWL_TRY(&frame.registers[0], bowl_string_utf8(&frame, buffer, strlen(buffer)));
    BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
    return NULL;
}
