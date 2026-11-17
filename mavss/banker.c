#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int *maximum;
    int *allocation;
    bool finished;
} Customer;

typedef struct {
    size_t resources;
    size_t count;
    int *available;
    int *work;
    Customer *customers;
} Banker;

static void skip_spaces(char **text)
{
    while (isspace((unsigned char)**text)) {
        ++*text;
    }
}

static bool read_number(char **text, int *value)
{
    if (**text < '0' || **text > '9') {
        return false;
    }

    char *end;
    errno = 0;
    long number = strtol(*text, &end, 10);
    if (errno == ERANGE || number > INT_MAX) {
        return false;
    }

    *value = (int)number;
    *text = end;
    return true;
}

static bool read_vector(char *text, int *values, size_t count, char separator)
{
    for (size_t i = 0; i < count; ++i) {
        if (i > 0 && separator == ',') {
            skip_spaces(&text);
            if (*text != ',') {
                return false;
            }
            ++text;
        } else if (i > 0 && !isspace((unsigned char)*text)) {
            return false;
        }

        skip_spaces(&text);
        if (!read_number(&text, &values[i])) {
            return false;
        }
    }

    skip_spaces(&text);
    return *text == '\0';
}

static Customer *add_customer(Banker *banker, size_t *capacity)
{
    if (banker->count == *capacity) {
        if (*capacity > SIZE_MAX / sizeof(Customer) / 2) {
            return NULL;
        }

        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        Customer *customers = realloc(banker->customers, next * sizeof(*customers));
        if (customers == NULL) {
            return NULL;
        }

        banker->customers = customers;
        *capacity = next;
    }

    Customer *customer = &banker->customers[banker->count++];
    customer->maximum = calloc(banker->resources, sizeof(*customer->maximum));
    customer->allocation = calloc(banker->resources, sizeof(*customer->allocation));
    if (customer->maximum == NULL || customer->allocation == NULL) {
        return NULL;
    }
    return customer;
}

static bool load_customers(Banker *banker)
{
    FILE *input = fopen("customer.txt", "r");
    if (input == NULL) {
        perror("customer.txt");
        return false;
    }

    char *line = NULL;
    size_t line_size = 0;
    size_t capacity = 0;
    ssize_t length;
    bool valid = true;

    while ((length = getline(&line, &line_size, input)) >= 0) {
        Customer *customer = add_customer(banker, &capacity);
        if (customer == NULL) {
            fputs("Not enough memory to load the customers\n", stderr);
            valid = false;
            break;
        }
        if (memchr(line, '\0', (size_t)length) != NULL ||
            !read_vector(line, customer->maximum, banker->resources, ',')) {
            fprintf(stderr, "customer.txt:%zu: expected %zu nonnegative integers separated by commas\n",
                    banker->count, banker->resources);
            valid = false;
            break;
        }
    }

    if (valid && (ferror(input) || !feof(input))) {
        perror("customer.txt");
        valid = false;
    }
    if (valid && banker->count == 0) {
        fputs("customer.txt: expected at least one customer\n", stderr);
        valid = false;
    }

    free(line);
    fclose(input);
    return valid;
}

static bool can_finish(const Customer *customer, const int *resources, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (customer->maximum[i] - customer->allocation[i] > resources[i]) {
            return false;
        }
    }
    return true;
}

static bool fits(const int *amounts, const int *limit, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (amounts[i] > limit[i]) {
            return false;
        }
    }
    return true;
}

static bool is_safe(Banker *banker)
{
    memcpy(banker->work, banker->available, banker->resources * sizeof(*banker->work));
    for (size_t i = 0; i < banker->count; ++i) {
        banker->customers[i].finished = false;
    }

    size_t remaining = banker->count;
    while (remaining > 0) {
        size_t before = remaining;
        for (size_t i = 0; i < banker->count; ++i) {
            Customer *customer = &banker->customers[i];
            if (customer->finished || !can_finish(customer, banker->work, banker->resources)) {
                continue;
            }

            for (size_t j = 0; j < banker->resources; ++j) {
                banker->work[j] += customer->allocation[j];
            }
            customer->finished = true;
            --remaining;
        }
        if (remaining == before) {
            return false;
        }
    }
    return true;
}

static void transfer(Banker *banker, Customer *customer, const int *amounts, int direction)
{
    for (size_t i = 0; i < banker->resources; ++i) {
        int amount = direction * amounts[i];
        banker->available[i] -= amount;
        customer->allocation[i] += amount;
    }
}

static void print_vector(FILE *output, const int *values, size_t count)
{
    fprintf(output, "%d", values[0]);
    for (size_t i = 1; i < count; ++i) {
        fprintf(output, " %d", values[i]);
    }
}

static void print_state(const Banker *banker, FILE *output)
{
    fputs("MAXIMUM | ALLOCATION | NEED\n", output);
    for (size_t i = 0; i < banker->count; ++i) {
        const Customer *customer = &banker->customers[i];
        print_vector(output, customer->maximum, banker->resources);
        fputs(" | ", output);
        print_vector(output, customer->allocation, banker->resources);
        fprintf(output, " | %d", customer->maximum[0] - customer->allocation[0]);
        for (size_t j = 1; j < banker->resources; ++j) {
            fprintf(output, " %d", customer->maximum[j] - customer->allocation[j]);
        }
        fputc('\n', output);
    }
    fputs("AVAILABLE ", output);
    print_vector(output, banker->available, banker->resources);
    fputc('\n', output);
}

static void request_resources(Banker *banker, FILE *output, int id, const int *amounts)
{
    Customer *customer = &banker->customers[id];
    const char *denial = NULL;

    for (size_t i = 0; i < banker->resources; ++i) {
        if (amounts[i] > customer->maximum[i] - customer->allocation[i]) {
            denial = "exceed its maximum allocation";
            break;
        }
    }
    if (denial == NULL && !fits(amounts, banker->available, banker->resources)) {
        fputs("The resources ", output);
        print_vector(output, banker->available, banker->resources);
        fprintf(output, " are not enough to customer %d request ", id);
        print_vector(output, amounts, banker->resources);
        fputc('\n', output);
        return;
    }
    if (denial == NULL) {
        transfer(banker, customer, amounts, 1);
        if (!is_safe(banker)) {
            transfer(banker, customer, amounts, -1);
            denial = "result in an unsafe state";
        }
    }

    if (denial != NULL) {
        fprintf(output, "The customer %d request ", id);
        print_vector(output, amounts, banker->resources);
        fprintf(output, " was denied because %s\n", denial);
    } else {
        fprintf(output, "Allocate to customer %d the resources ", id);
        print_vector(output, amounts, banker->resources);
        fputc('\n', output);
    }
}

static void release_resources(Banker *banker, FILE *output, int id, const int *amounts)
{
    Customer *customer = &banker->customers[id];
    if (!fits(amounts, customer->allocation, banker->resources)) {
        fprintf(output, "The customer %d release ", id);
        print_vector(output, amounts, banker->resources);
        fputs(" was denied because it exceeds its current allocation\n", output);
        return;
    }

    transfer(banker, customer, amounts, -1);
    fprintf(output, "Release from customer %d the resources ", id);
    print_vector(output, amounts, banker->resources);
    fputc('\n', output);
}

static bool run_command(Banker *banker, FILE *output, char *line, int *amounts)
{
    skip_spaces(&line);
    if (*line == '*') {
        ++line;
        skip_spaces(&line);
        if (*line != '\0') {
            return false;
        }
        print_state(banker, output);
        return true;
    }

    bool request = strncmp(line, "RQ", 2) == 0;
    bool release = strncmp(line, "RL", 2) == 0;
    if ((!request && !release) || !isspace((unsigned char)line[2])) {
        return false;
    }

    line += 2;
    skip_spaces(&line);
    int id;
    if (!read_number(&line, &id) || (size_t)id >= banker->count ||
        !isspace((unsigned char)*line) || !read_vector(line, amounts, banker->resources, ' ')) {
        return false;
    }

    if (request) {
        request_resources(banker, output, id, amounts);
    } else {
        release_resources(banker, output, id, amounts);
    }
    return true;
}

static bool run_commands(Banker *banker, FILE *input, FILE *output)
{
    int *amounts = calloc(banker->resources, sizeof(*amounts));
    if (amounts == NULL) {
        fputs("Not enough memory to read the commands\n", stderr);
        return false;
    }

    char *line = NULL;
    size_t line_size = 0;
    size_t number = 0;
    ssize_t length;
    bool valid = true;

    while ((length = getline(&line, &line_size, input)) >= 0) {
        ++number;
        if (memchr(line, '\0', (size_t)length) != NULL || !run_command(banker, output, line, amounts)) {
            fprintf(stderr, "commands.txt:%zu: expected * or RQ/RL with a valid customer and %zu nonnegative resource amounts\n",
                    number, banker->resources);
            valid = false;
            break;
        }
    }

    if (valid && (ferror(input) || !feof(input))) {
        perror("commands.txt");
        valid = false;
    }

    free(line);
    free(amounts);
    return valid;
}

static bool process_files(Banker *banker)
{
    if (!load_customers(banker)) {
        return false;
    }

    FILE *input = fopen("commands.txt", "r");
    if (input == NULL) {
        perror("commands.txt");
        return false;
    }

    FILE *output = fopen("result.txt", "w");
    if (output == NULL) {
        perror("result.txt");
        fclose(input);
        return false;
    }

    bool valid = run_commands(banker, input, output);
    int write_failed = ferror(output);
    if (fclose(output) == EOF || write_failed) {
        fputs("Could not write result.txt\n", stderr);
        valid = false;
    }

    fclose(input);
    return valid;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s resource_count [resource_count ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    Banker banker = {.resources = (size_t)(argc - 1)};
    banker.available = calloc(banker.resources, sizeof(*banker.available));
    banker.work = calloc(banker.resources, sizeof(*banker.work));
    bool valid = banker.available != NULL && banker.work != NULL;
    if (!valid) {
        fputs("Not enough memory to initialize the resources\n", stderr);
    }

    for (int i = 1; valid && i < argc; ++i) {
        char *argument = argv[i];
        if (!read_number(&argument, &banker.available[i - 1]) || *argument != '\0') {
            fprintf(stderr, "Argument %d: expected a nonnegative integer\n", i);
            valid = false;
        }
    }
    if (valid) {
        valid = process_files(&banker);
    }

    for (size_t i = 0; i < banker.count; ++i) {
        free(banker.customers[i].maximum);
        free(banker.customers[i].allocation);
    }
    free(banker.customers);
    free(banker.available);
    free(banker.work);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
