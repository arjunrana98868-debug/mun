// cv_parser.cpp
// Reads a CV (plain text) and reports:
//   1. number of MUNs (Model United Nations conferences) the person has done
//   2. the location the person is based in
//
// Build:  g++ -std=c++17 -O2 cv_parser.cpp -o cv_parser
// Run:    ./cv_parser cv.txt
//
// PDF CVs: convert first, e.g.  pdftotext -layout cv.pdf cv.txt
//
// Approach (heuristic, rule based - no ML):
//   MUN count : every distinct CV line that mentions a MUN AND looks like an
//               entry (has a year or a role word) counts as one MUN.
//               Summary claims like "attended 12+ MUNs" are read separately.
//               Final answer = max(entries found, claimed number).
//   Location  : explicit label ("Location: ..."), else first known city
//               in the header area (top 15 lines), else first known city anywhere.

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// ---------- helpers ----------

static string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c) { return tolower(c); });
    return s;
}

static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Collapse to letters/digits/single spaces so duplicate lines compare equal.
static string normalize(const string &s) {
    string out;
    bool lastSpace = true;
    for (unsigned char c : s) {
        if (isalnum(c)) { out += (char)tolower(c); lastSpace = false; }
        else if (!lastSpace) { out += ' '; lastSpace = true; }
    }
    return trim(out);
}

static vector<string> readLines(const string &path) {
    ifstream in(path);
    vector<string> lines;
    string line;
    while (getline(in, line)) lines.push_back(line);
    return lines;
}

// ---------- MUN counting ----------

// Does the line mention a MUN? Matches "MUN", "MUNs", "IITMUN", "MUN'24",
// "Model United Nations". Ignores words like "commun..." / "immun...".
static bool mentionsMun(const string &lower) {
    if (lower.find("model united nations") != string::npos) return true;
    static const regex tok("\\b([a-z]*)mun(s)?\\b");
    for (sregex_iterator it(lower.begin(), lower.end(), tok), end; it != end; ++it) {
        string prefix = (*it)[1];
        if (prefix == "com" || prefix == "im" || prefix == "ra") continue;
        return true;
    }
    return false;
}

// Looks like an actual entry (a specific conference), not a heading.
static bool looksLikeEntry(const string &lower) {
    static const regex year("\\b(19|20)\\d\\d\\b|'\\d\\d\\b");
    static const regex role(
        "\\b(delegate|chair|co-chair|vice[- ]chair|director|rapporteur|"
        "secretary[- ]general|deputy|executive board|eb member|"
        "participated|participant|attended|represented|award|best|"
        "special mention|verbal mention|high commendation|organi[sz]ed|"
        "organizing committee|usg|dias)\\b");
    return regex_search(lower, year) || regex_search(lower, role);
}

// "attended 12 MUNs", "15+ MUN conferences", "participated in 8 Model UN"
static int claimedCount(const string &lower) {
    static const regex claim(
        "(\\d{1,3})\\s*\\+?\\s*(?:[a-z]+\\s+){0,2}"
        "(?:muns?|model united nations|model un)\\b");
    smatch m;
    int best = 0;
    string s = lower;
    while (regex_search(s, m, claim)) {
        best = max(best, stoi(m[1]));
        s = m.suffix();
    }
    // Ignore things that are really years ("MUN 2023" is not 2023 MUNs)
    return best;
}

struct MunResult {
    int entries = 0;
    int claimed = 0;
    int total = 0;
    vector<string> matched;
};

static MunResult countMuns(const vector<string> &lines) {
    MunResult r;
    set<string> seen;
    for (const string &raw : lines) {
        string line = trim(raw);
        if (line.empty()) continue;
        string lower = toLower(line);
        if (!mentionsMun(lower)) continue;

        int c = claimedCount(lower);
        if (c > 0) {
            r.claimed = max(r.claimed, c);
            continue;  // a summary sentence, not a single conference
        }
        if (!looksLikeEntry(lower)) continue;

        string key = normalize(line);
        if (seen.insert(key).second) {
            r.entries++;
            r.matched.push_back(line);
        }
    }
    r.total = max(r.entries, r.claimed);
    return r;
}

// ---------- location ----------

static const vector<string> kPlaces = {
    // Uttarakhand (adds local coverage)
    "dehradun", "nainital", "naini tal", "haldwani", "haridwar", "rishikesh",
    "roorkee", "kashipur", "rudrapur", "almora", "mussoorie", "pantnagar",
    // major Indian cities
    "delhi", "new delhi", "noida", "gurgaon", "gurugram", "faridabad",
    "ghaziabad", "mumbai", "pune", "bengaluru", "bangalore", "hyderabad",
    "chennai", "kolkata", "ahmedabad", "surat", "jaipur", "lucknow", "kanpur",
    "nagpur", "indore", "bhopal", "patna", "chandigarh", "ludhiana", "amritsar",
    "coimbatore", "kochi", "thiruvananthapuram", "visakhapatnam", "vadodara",
    "guwahati", "bhubaneswar", "ranchi", "raipur", "mysuru", "mysore",
    "varanasi", "agra", "meerut", "jodhpur", "udaipur", "shimla", "jammu",
    "srinagar", "goa", "panaji", "mangalore", "madurai", "vellore", "manipal",
    "greater noida",
    // states / countries (fallback)
    "uttarakhand", "uttar pradesh", "maharashtra", "karnataka", "tamil nadu",
    "rajasthan", "gujarat", "punjab", "haryana", "kerala", "west bengal",
    "india", "usa", "united states", "uk", "united kingdom", "canada",
    "australia", "singapore", "dubai", "uae", "germany", "france"};

static string titleCase(string s) {
    bool up = true;
    for (char &c : s) {
        if (up && isalpha((unsigned char)c)) { c = toupper(c); up = false; }
        if (c == ' ') up = true;
    }
    return s;
}

// Find the first known place in `text`, as a whole word/phrase.
// Prefers longer names so "new delhi" beats "delhi".
static string findPlace(const string &text) {
    string lower = toLower(text);
    string best;
    size_t bestPos = string::npos;
    for (const string &p : kPlaces) {
        regex re("\\b" + p + "\\b");
        smatch m;
        if (regex_search(lower, m, re)) {
            size_t pos = m.position(0);
            if (pos < bestPos || (pos == bestPos && p.size() > best.size())) {
                bestPos = pos;
                best = p;
            }
        }
    }
    return best.empty() ? "" : titleCase(best);
}

static string findLocation(const vector<string> &lines) {
    // 1) Explicit label: "Location: X", "Address: X", "Based in X", "City: X"
    static const regex label(
        "^\\s*(?:location|address|city|based in|current location|residence)"
        "\\s*[:\\-]\\s*(.+)$",
        regex::icase);
    for (const string &l : lines) {
        smatch m;
        if (regex_search(l, m, label)) {
            string val = trim(m[1]);
            if (!val.empty()) return val;
        }
    }
    // 2) Header area (name / contact block is normally in the first lines)
    string header;
    for (size_t i = 0; i < lines.size() && i < 15; i++) header += lines[i] + "\n";
    string p = findPlace(header);
    if (!p.empty()) return p;

    // 3) Anywhere in the CV
    string all;
    for (const string &l : lines) all += l + "\n";
    return findPlace(all);
}

// ---------- main ----------

static string jsonEscape(const string &s) {
    string o;
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r' || c == '\t') o += ' ';
        else if (c < 0x20) o += ' ';
        else o += (char)c;
    }
    return o;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <cv.txt> [--json]\n";
        return 1;
    }
    bool asJson = (argc >= 3 && string(argv[2]) == "--json");

    ifstream probe(argv[1]);
    if (!probe) {
        cerr << "File not found: " << argv[1] << "\n";
        return 1;
    }
    probe.close();

    vector<string> lines = readLines(argv[1]);
    if (lines.empty()) {
        cerr << "File is empty: " << argv[1] << "\n";
        return 1;
    }

    MunResult mun = countMuns(lines);
    string loc = findLocation(lines);

    if (asJson) {
        cout << "{\"mun_count\":" << mun.total
             << ",\"entries_found\":" << mun.entries
             << ",\"claimed\":" << mun.claimed
             << ",\"location\":\"" << jsonEscape(loc) << "\""
             << ",\"matched\":[";
        for (size_t i = 0; i < mun.matched.size(); i++) {
            if (i) cout << ",";
            cout << "\"" << jsonEscape(trim(mun.matched[i])) << "\"";
        }
        cout << "]}\n";
        return 0;
    }

    cout << "MUNs done : " << mun.total << "\n";
    cout << "Location  : " << (loc.empty() ? "Not found" : loc) << "\n";
    cout << "\n--- details ---\n";
    cout << "Entries found : " << mun.entries << "\n";
    cout << "Claimed in CV : " << mun.claimed << "\n";
    for (const string &m : mun.matched) cout << "  * " << trim(m) << "\n";
    return 0;
}
