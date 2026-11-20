// Host test for the BLE seam's codecs in the Toit fork (ztoit #41): rete's relay frame
// (copied verbatim) against rete's golden vector, and the IPv6 text codec.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "relay_frame.h"
#include "ipv6_text.h"

static int failures = 0;
#define CHECK(name, cond) do { if (cond) printf("PASS %s\n", name); \
  else { printf("FAIL %s (line %d)\n", name, __LINE__); failures++; } } while (0)

static std::string slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str();
}
static std::vector<uint8_t> golden(const std::string& rete) {
  std::string hex = slurp(rete + "/protocol/fixtures/relay/frame.hex");
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    if (!isxdigit(hex[i])) break;
    out.push_back((uint8_t)std::stoi(hex.substr(i, 2), nullptr, 16));
  }
  return out;
}
static const uint8_t ECHO[16] = {0xfd,0x7a,0x11,0x5c,0xa1,0xe0,0,0,0,0,0,0,0xbf,0x01,0xd9,0x21};

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: test_ble_codecs <rete> <fork-resources>\n"); return 2; }
  std::string rete = argv[1], fork = argv[2];
  std::vector<uint8_t> v = golden(rete);

  struct relay_frame f;
  bool ok = !v.empty() && relay_frame_parse(v.data(), v.size(), &f) == 0;
  CHECK("golden_vector_parses", ok && f.local_port == 16970 && f.remote_port == 6969 &&
        std::string((const char*)v.data() + f.host_off, f.host_len) == "fd7a:115c:a1e0::1" &&
        f.payload_len == 2 && memcmp(v.data() + f.payload_off, "hi", 2) == 0);

  uint8_t buf[64];
  int h = relay_frame_put_header(buf, sizeof buf, 16970, 6969, "fd7a:115c:a1e0::1", 17);
  bool built = h > 0 && (size_t)h + 2 <= sizeof buf;
  if (built) memcpy(buf + h, "hi", 2);
  CHECK("golden_vector_builds", built && (size_t)h + 2 == v.size() && memcmp(buf, v.data(), v.size()) == 0);

  char text[IPV6_TEXT_MAX];
  int n = ipv6_format(ECHO, text, sizeof text);
  CHECK("format_drops_leading_zeros", n > 0 && std::string(text, n) == "fd7a:115c:a1e0:0:0:0:bf01:d921" &&
        text[n] == 0);

  uint8_t loop[16] = {}, ffff[16], zero[16] = {}, one[16] = {}, back[16];
  memset(ffff, 0xff, 16); one[15] = 1;
  const uint8_t* cases[] = {ECHO, zero, one, ffff};
  bool rt = true;
  for (auto c : cases) {
    int k = ipv6_format(c, text, sizeof text);
    rt = rt && k > 0 && ipv6_parse(text, k, back) == 0 && memcmp(back, c, 16) == 0;
  }
  CHECK("parse_round_trips", rt);

  CHECK("parse_compressed",
        ipv6_parse("fd7a:115c:a1e0::bf01:d921", 25, loop) == 0 && memcmp(loop, ECHO, 16) == 0 &&
        ipv6_parse("::1", 3, back) == 0 && memcmp(back, one, 16) == 0 &&
        ipv6_parse("::", 2, back) == 0 && memcmp(back, zero, 16) == 0);

  const char* bad[] = {"100.87.217.33", "1:2:3", "1::2::3", "12345::", "g::", "", ":1::", "1:2:3:4:5:6:7:8:9",
                       "1:2:3:4:5:6:7:8::", "1:"};
  bool rejects = true;
  for (auto b : bad) {
    if (ipv6_parse(b, strlen(b), back) != -1) { printf("  accepted \"%s\"\n", b); rejects = false; }
  }
  CHECK("parse_rejects", rejects);

  CHECK("format_short_buffer", ipv6_format(ECHO, text, 10) == -1);

  CHECK("copy_matches_rete", slurp(fork + "/relay_frame.h") == slurp(rete + "/nodus/oevm/src/relay_frame.h") &&
        !slurp(fork + "/relay_frame.h").empty());

  printf("%s\n", failures ? "FAILED" : "ALL PASS");
  return failures ? 1 : 0;
}
