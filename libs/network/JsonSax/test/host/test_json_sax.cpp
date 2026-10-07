#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "StreamingJsonParser.h"

namespace {
struct Recorder {
  std::vector<std::string> events;
  unsigned overflows = 0;
};

void onKey(void* ctx, const char* key, size_t len) {
  static_cast<Recorder*>(ctx)->events.push_back("k:" + std::string(key, len));
}
void onString(void* ctx, const char* value, size_t len) {
  static_cast<Recorder*>(ctx)->events.push_back("s:" + std::string(value, len));
}
void onNumber(void* ctx, const char* value, size_t len) {
  static_cast<Recorder*>(ctx)->events.push_back("n:" + std::string(value, len));
}
void onOverflow(void* ctx) { ++static_cast<Recorder*>(ctx)->overflows; }

JsonCallbacks callbacks(Recorder& recorder, bool reportOverflow) {
  JsonCallbacks cb{};
  cb.ctx = &recorder;
  cb.onKey = onKey;
  cb.onString = onString;
  cb.onNumber = onNumber;
  cb.onOverflow = reportOverflow ? onOverflow : nullptr;
  return cb;
}

void feed(JsonSaxParser& parser, const std::string& json) { parser.feed(json.data(), json.size()); }

void testCallerBufferCapacity() {
  // Capacity 6 holds five bytes plus the terminator.
  char buffer[6];
  Recorder recorder;
  JsonSaxParser parser(callbacks(recorder, true), buffer, sizeof(buffer));
  feed(parser, R"({"abcde":"12345","x":123456})");
  assert(!parser.hasError());
  assert((recorder.events == std::vector<std::string>{"k:abcde", "s:12345", "k:x"}));
  assert(recorder.overflows == 1);  // the six-digit number
}

void testOverflowReportsEveryTokenKindAndContinues() {
  char buffer[4];
  Recorder recorder;
  JsonSaxParser parser(callbacks(recorder, true), buffer, sizeof(buffer));
  feed(parser, R"({"long-key":"ok","k":"long-value","n":12345,"tail":"end"})");
  assert(!parser.hasError());
  assert((recorder.events == std::vector<std::string>{"s:ok", "k:k", "k:n", "s:end"}));
  assert(recorder.overflows == 4);  // "long-key", "long-value", 12345, "tail"
}

void testOverflowWithoutCallbackStaysSilent() {
  char buffer[4];
  Recorder recorder;
  JsonSaxParser parser(callbacks(recorder, false), buffer, sizeof(buffer));
  feed(parser, R"({"k":"long-value"})");
  assert(!parser.hasError());
  assert((recorder.events == std::vector<std::string>{"k:k"}));
  assert(recorder.overflows == 0);
}

void testInvalidBufferFails() {
  char one[1];
  for (auto [buffer, capacity] : {std::pair<char*, size_t>{nullptr, 64}, {one, 1}, {one, 0}}) {
    Recorder recorder;
    JsonSaxParser parser(callbacks(recorder, true), buffer, capacity);
    feed(parser, R"({"k":""})");
    assert(parser.hasError());
    assert(recorder.events.empty());
  }
}

void testInlineParserUnchanged() {
  Recorder recorder;
  StreamingJsonParser parser(callbacks(recorder, true));
  const std::string fits(StreamingJsonParser::TOKEN_BUF_SIZE - 1, 'a');
  const std::string tooLong(StreamingJsonParser::TOKEN_BUF_SIZE, 'b');
  feed(parser, "[\"" + fits + "\",\"" + tooLong + "\"]");
  assert(!parser.hasError());
  assert((recorder.events == std::vector<std::string>{"s:" + fits}));
  assert(recorder.overflows == 1);
}

// The core carries no token storage; only the inline wrapper does.
static_assert(sizeof(StreamingJsonParser) >= sizeof(JsonSaxParser) + StreamingJsonParser::TOKEN_BUF_SIZE);
static_assert(sizeof(JsonSaxParser) < 256);
}  // namespace

int main() {
  testCallerBufferCapacity();
  testOverflowReportsEveryTokenKindAndContinues();
  testOverflowWithoutCallbackStaysSilent();
  testInvalidBufferFails();
  testInlineParserUnchanged();
  std::puts("jsonsax: all tests passed");
  return 0;
}
