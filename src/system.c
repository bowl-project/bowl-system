#include "system.h"

static BowlFunctionEntry system_functions[] = {
    { .name = "system:exit", .documentation = "", .function = system_exit },
    { .name = "system:chdir", .documentation = "", .function = system_change_directory },
    { .name = "system:dir", .documentation = "", .function = system_directory },
    { .name = "system:execute", .documentation = "", .function = system_execute },
    { .name = "system:wait", .documentation = "", .function = system_wait }
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

BowlValue system_execute(BowlStack stack) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);

    BOWL_STACK_POP_VALUE(&frame, &frame.registers[1]);
    BOWL_ASSERT_TYPE(frame.registers[1], BowlListValue);

    BOWL_STACK_POP_VALUE(&frame, &frame.registers[0]);
    BOWL_ASSERT_TYPE(frame.registers[0], BowlStringValue);

    char *program = unicode_to_string(&frame.registers[0]->string.codepoints[0], frame.registers[0]->string.length);
    if (program == NULL) {
        return bowl_exception_out_of_heap;
    }

    char **arguments = malloc(sizeof(char *) * (bowl_value_length(frame.registers[1]) + 2));
    u64 p = 0;
    if (arguments == NULL) {
        free(program);
        return bowl_exception_out_of_heap;
    }

    // the first argument is the process that is going to be executed
    arguments[p++] = program;

    BowlValue argument_list = frame.registers[1];
    while (argument_list != NULL) {
        if (argument_list->list.head == NULL || argument_list->list.head->type != BowlStringValue) {
            for (u64 i = 0; i < p; ++i) {
                free(arguments[i]);
            }
            free(arguments);
            return bowl_format_exception(&frame, "illegal argument in function '%s' (expected a list of string arguments)", __FUNCTION__).value;
        }

        arguments[p] = unicode_to_string(&argument_list->list.head->string.codepoints[0], argument_list->list.head->string.length);
        
        if (arguments[p] == NULL) {
            for (u64 i = 0; i < p; ++i) {
                free(arguments[i]);
            }
            free(arguments);
            return bowl_exception_out_of_heap;
        }

        ++p;

        argument_list = argument_list->list.tail;
    }

    // the argument array must be null-terminated
    arguments[p] = NULL;

    #if defined(OS_UNIX)
        const s64 pid = fork();
        if (pid == 0) {
            // child process
            execvp(program, arguments);
            exit(EXIT_FAILURE);
        } else if (pid != -1) {
            // parent process

            for (u64 i = 0; i < p; ++i) {
                free(arguments[i]);
            }
            free(arguments);
            
            BOWL_TRY(&frame.registers[0], bowl_number(&frame, (double) pid));
            BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
        } else {
            // error
            return bowl_format_exception(&frame, "failed to execute process").value;
        }
    #elif defined(OS_WINDOWS)
        
    #endif

    return NULL;
}

BowlValue system_wait(BowlStack stack) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);

    BOWL_STACK_POP_VALUE(&frame, &frame.registers[0]);
    BOWL_ASSERT_TYPE(frame.registers[0], BowlNumberValue);

    const s64 pid = (s64) frame.registers[0]->number.value;

    #if defined(OS_UNIX)
        if (pid > 0) {
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                return bowl_format_exception(&frame, "failed to wait for process with id #%" PRId64, pid).value;
            } else if (WIFEXITED(status)) {
                BOWL_TRY(&frame.registers[0], bowl_number(&frame, (double) WEXITSTATUS(status)));
                BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
            } else if (WIFSIGNALED(status)) {
                BOWL_TRY(&frame.registers[0], bowl_number(&frame, (double) WTERMSIG(status)));
                BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
            } else if (WIFSTOPPED(status)) {
                BOWL_TRY(&frame.registers[0], bowl_number(&frame, (double) WSTOPSIG(status)));
                BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
            } else {
                return bowl_format_exception(&frame, "illegal process status").value;
            }
        } else {
            return bowl_format_exception(&frame, "illegal process id #%" PRId64 " provided", pid).value;
        }
    #elif defined(OS_WINDOWS)
        
    #endif

    return NULL;
}
