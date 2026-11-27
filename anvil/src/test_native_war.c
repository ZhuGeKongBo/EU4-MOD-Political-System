#include "native_war.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, submissions;
static bool permitted;

static unsigned field(const void *object, size_t offset) {
    uint32_t value;
    memcpy(&value, (const unsigned char *)object + offset, sizeof(value));
    return value;
}

static bool eligible(const void *command, GameString *message, bool display) {
    assert(!display && message->capacity == 15);
    assert(field(command, 0x08) == 0x165);
    assert(field(command, 0x10) == 1 && field(command, 0x18) == 1 && field(command, 0x20) == 2);
    assert(field(command, 0x40) == 3 && field(command, 0x4C) == 0x13);
    assert(field(command, 0x28) == 44 && field(command, 0x34) == 0);
    ++checks;
    return permitted;
}

static void *clone_command(const void *command) {
    assert(field(command, 0x34) == 2);
    void *copy = malloc(0x70);
    assert(copy);
    memcpy(copy, command, 0x70);
    return copy;
}

static void *payload(const void *envelope) {
    void *command;
    memcpy(&command, (const unsigned char *)envelope + 0x50, sizeof(command));
    return command;
}

static void submit(void *envelope) {
    assert(field(envelope, 0x44) == 0x2934 && field(payload(envelope), 0x34) == 2);
    ++submissions;
}

static void *dispose(void *envelope, unsigned free_storage) {
    assert(!free_storage);
    free(payload(envelope));
    return envelope;
}

static void free_message(GameString *message) {
    assert(message->capacity == 15);
}

int main(void) {
    NativeDefense api = {.check = eligible, .clone = clone_command, .submit = submit,
        .dispose = dispose, .free_string = free_message};
    assert(!summon_defender(&api, 1, 1, 3, 44));
    assert(!summon_defender(&api, 1, 2, 3, 44) && checks == 1 && submissions == 0);
    permitted = true;
    assert(summon_defender(&api, 1, 2, 3, 44) && checks == 2 && submissions == 1);
    puts("PASS exact defender/member/opponent fields, native eligibility refusal and accepted defensive command ownership.");
}
