#pragma once

#include <cstddef>
#include <cstdint>

struct JsonCallbacks {
  void* ctx;
  void (*onKey)(void* ctx, const char* key, size_t len);
  void (*onString)(void* ctx, const char* value, size_t len);
  void (*onNumber)(void* ctx, const char* value, size_t len);
  void (*onBool)(void* ctx, bool value);
  void (*onNull)(void* ctx);
  void (*onObjectStart)(void* ctx);
  void (*onObjectEnd)(void* ctx);
  void (*onArrayStart)(void* ctx);
  void (*onArrayEnd)(void* ctx);
  // Optional: called once for each key, string or number dropped because it
  // did not fit the token buffer. Without it, oversized tokens vanish silently.
  void (*onOverflow)(void* ctx) = nullptr;
};

// SAX parser over caller-owned token storage. Each key, string or number must
// fit in tokenBuffer (capacity includes the terminator); longer ones are
// dropped and reported through onOverflow. A null buffer or capacity below 2
// makes feed() fail with hasError().
class JsonSaxParser {
 public:
  static constexpr size_t MAX_NESTING = 32;

  JsonSaxParser(const JsonCallbacks& callbacks, char* tokenBuffer, size_t capacity);
  JsonSaxParser(const JsonSaxParser&) = delete;
  JsonSaxParser& operator=(const JsonSaxParser&) = delete;

  void reset();
  void feed(const char* data, size_t len);

  bool hasError() const { return error; }

 private:
  enum class State : uint8_t {
    SCANNING,
    IN_STRING_KEY,
    IN_STRING_VALUE,
    IN_NUMBER,
    IN_LITERAL,
    SKIP_STRING,
  };

  enum class Container : uint8_t {
    NONE,
    OBJECT,
    ARRAY,
  };

  void handleScanning(char c);
  void handleStringChar(char c);
  void handleNumber(char c);
  void handleLiteral(char c);
  void handleSkipString(char c);

  void appendToken(char c);
  void appendUtf8(uint32_t codepoint);
  void finishUnicodeEscape();
  void flushPendingSurrogate();
  void emitToken();

  bool inArray() const { return nestingDepth > 0 && nestingStack[nestingDepth - 1] == Container::ARRAY; }

  JsonCallbacks cb;
  char* tokenBuf;
  size_t tokenCapacity;
  size_t tokenLen;
  State state;
  bool expectingValue;
  bool escaped;
  bool tokenOverflow;
  bool error;

  Container nestingStack[MAX_NESTING];
  uint8_t nestingDepth;

  char literalExpected[6];
  uint8_t literalLen;
  uint8_t literalPos;

  // \uXXXX escape decoding (UTF-16, including surrogate pairs) to UTF-8.
  uint8_t unicodeDigitsLeft;
  uint16_t unicodeValue;
  uint16_t pendingHighSurrogate;
};

namespace jsonsax_detail {
constexpr size_t INLINE_TOKEN_BYTES = 2048;
// Separate base so the inline buffer is constructed before JsonSaxParser.
struct InlineTokenBuffer {
  char tokenStorage[INLINE_TOKEN_BYTES];
};
}  // namespace jsonsax_detail

// JsonSaxParser with its own inline token buffer.
class StreamingJsonParser : private jsonsax_detail::InlineTokenBuffer, public JsonSaxParser {
 public:
  // Sized so the longest value the OPDS parsers accept fits without tripping
  // tokenOverflow, which drops the whole string. This must cover not just URLs
  // (768-char hrefs/templates, see OpdsLimits) but the longest text field: a
  // publication description (capped at 2048 by the detail parser). Undersizing
  // silently drops long descriptions rather than truncating them.
  static constexpr size_t TOKEN_BUF_SIZE = jsonsax_detail::INLINE_TOKEN_BYTES;

  explicit StreamingJsonParser(const JsonCallbacks& callbacks)
      : JsonSaxParser(callbacks, tokenStorage, TOKEN_BUF_SIZE) {}
};
