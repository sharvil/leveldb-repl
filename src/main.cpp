#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "leveldb/db.h"
#include "linenoise.h"

#define ARRAY_SIZE(x) (sizeof(x) / (sizeof((x)[0])))

using namespace leveldb;

static void usage(const char *name);
static void trim(char *string);
static void autocomplete(const char *prefix, linenoiseCompletions *completions);
static void del(const char *args);
static void get(const char *args);
static void list(const char *args);
static void rlist(const char *args);
static void set(const char *args);
static void help(const char *args);

static char *parseString(const char **inputString);

struct {
  const char *name;
  const char *args;
  const char *description;
  void (*handler)(const char *args);
} static const COMMANDS[] = {
  { "del", "<key>", "Deletes <key> from the database.", del },
  { "get", "<key>", "Prints the value of <key>.", get },
  { "list", "[start] [end]", "Prints out keys in the range [start, end] inclusive.", list },
  { "rlist", "[start] [end]", "Prints out keys in the range [start, end] inclusive in reverse key order.", rlist },
  { "set", "<key> <value>", "Sets the <value> of <key>.", set },
  { "help", "[command]", "Shows help about the specified command.", help },
};

static const size_t LINE_MAX = 4096;
static DB *database;

int main(int argc, char **argv) {
  if (argc != 2)
    usage(argv[0]);

  Options options;
  options.create_if_missing = true;

  Status status = DB::Open(options, argv[1], &database);
  if (!status.ok()) {
    printf("Error opening database '%s': %s\n", argv[1], status.ToString().c_str());
    return 1;
  }

  char *line;
  linenoiseSetCompletionCallback(autocomplete);
  while ((line = linenoise("> "))) {
    trim(line);
    if (!line[0])
      continue;

    char command[LINE_MAX] = { 0 };
    sscanf(line, "%s", command);

    bool recognized = false;
    for (size_t i = 0; i < ARRAY_SIZE(COMMANDS); ++i) {
      if (!strcmp(COMMANDS[i].name, command)) {
        COMMANDS[i].handler(strstr(line, command) + strlen(command));
        recognized = true;
        break;
      }
    }

    if (!recognized)
      printf("Unrecognized command '%s'.\n", command);

    linenoiseHistoryAdd(line);
    free(line);
  }

  delete database;
  return 0;
}

static void usage(const char *name) {
  printf("Usage: %s <database>\n", name);
  exit(-1);
}

static void trim(char *string) {
  size_t start = 0;
  size_t end = strlen(string);

  while (start < end && isspace(string[start]))
    ++start;

  while (start < end && isspace(string[end - 1]))
    --end;

  memmove(&string[0], &string[start], end - start);
  string[end - start] = '\0';
}

static void autocomplete(const char *prefix, linenoiseCompletions *completions) {
  for (size_t i = 0; i < ARRAY_SIZE(COMMANDS); ++i)
    if (!strncmp(COMMANDS[i].name, prefix, strlen(prefix)))
      linenoiseAddCompletion(completions, COMMANDS[i].name);
}

static void del(const char *args) {
  char *key = parseString(&args);

  Status status = database->Delete(WriteOptions(), key);
  if (!status.ok())
    printf("Error deleting '%s' from database: %s\n", key, status.ToString().c_str());

  free(key);
}

static void get(const char *args) {
  char *key = parseString(&args);

  if (!key) {
    printf("Invalid key specified.\n");
    return;
  }

  std::string value;
  Status status = database->Get(ReadOptions(), key, &value);
  if (!status.ok())
    printf("Error reading '%s' from database: %s\n", key, status.ToString().c_str());
  else
    printf("%s\n", value.c_str());

  free(key);
}

static void list(const char *args) {
  char *start = parseString(&args);
  char *end = parseString(&args);

  Iterator *iterator = database->NewIterator(ReadOptions());
  for (start ? iterator->Seek(start) : iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
    Slice key = iterator->key();
    if (end && key.compare(end) > 0)
      break;
    printf("%s\n", key.ToString().c_str());
  }

  delete iterator;
  free(end);
  free(start);
}

static void rlist(const char *args) {
  char *start = parseString(&args);
  char *end = parseString(&args);

  Iterator *iterator = database->NewIterator(ReadOptions());

  if (start) {
    iterator->Seek(start);
    if (!iterator->Valid())
      iterator->SeekToLast();
    else if (iterator->key().compare(start) > 0)
      iterator->Prev();
  } else {
    iterator->SeekToLast();
  }

  for (; iterator->Valid(); iterator->Prev()) {
    Slice key = iterator->key();
    if (end && key.compare(end) < 0)
      break;
    printf("%s\n", key.ToString().c_str());
  }

  delete iterator;
  free(end);
  free(start);
}

static void set(const char *args) {
  char *key = parseString(&args);
  char *value = parseString(&args);

  if (!key) {
    printf("Invalid key specified.\n");
    return;
  }

  if (!value)
    value = strdup("");

  Status status = database->Put(WriteOptions(), key, value);
  if (!status.ok())
    printf("Error inserting '%s' into database: %s\n", key, status.ToString().c_str());

  free(value);
  free(key);
}

static void help(const char *args) {
  char *command = parseString(&args);
  for (size_t i = 0; command && i < ARRAY_SIZE(COMMANDS); ++i)
    if (!strcmp(COMMANDS[i].name, command)) {
      printf("%-5s %-20s %s\n", COMMANDS[i].name, COMMANDS[i].args, COMMANDS[i].description);
      goto exit;
    }

  for (size_t i = 0; i < ARRAY_SIZE(COMMANDS); ++i)
    printf("%-5s %-20s %s\n", COMMANDS[i].name, COMMANDS[i].args, COMMANDS[i].description);

exit:
  free(command);
}

static char *parseString(const char **inputString) {
  size_t in = 0;
  char terminator = ' ';
  const char *input = *inputString;

  while (isspace(input[in]))
    ++in;

  switch (input[in]) {
    case '\0':
      return NULL;

    case '\'':
    case '\"':
      terminator = input[in];
      ++in;
      break;

    default:
      break;
  }

  char output[LINE_MAX] = { 0 };
  size_t out = 0;
  for (;;) {
    if (input[in] == terminator) {
      ++in;
      break;
    }

    // Hit end of input but we're waiting for a quote terminator
    // so the input must be invalid. Consume it all and return error.
    if (input[in] == '\0') {
      if (terminator == ' ')
        break;
      *inputString += in;
      return NULL;
    }

    if (input[in] == '\\') {
      ++in;
      switch (input[in]) {
        case '\'':
          output[out] = '\'';
          break;
        case '\"':
          output[out] = '\"';
          break;
        case '\\':
          output[out] = '\\';
          break;
        case '/':
          output[out] = '/';
          break;
        case 'b':
          output[out] = '\b';
          break;
        case 'f':
          output[out] = '\f';
          break;
        case 'n':
          output[out] = '\n';
          break;
        case 'r':
          output[out] = '\r';
          break;
        case 't':
          output[out] = '\t';
          break;
        default:
          *inputString += in;
          return NULL;
      }
    } else {
      output[out] = input[in];
    }
    ++out;
    ++in;
  }

  *inputString += in;
  return strdup(output);
}
