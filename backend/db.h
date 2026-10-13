// db.h
// MySQL storage for the Committee & Portfolio module.
// Working data stays in the `committees` vector; every change is written to MySQL,
// and everything is loaded back from MySQL when the committee menu is first opened.
//
// Connection settings come from environment variables (do NOT hard-code passwords
// in code you push to GitHub):
//   MUN_DB_HOST (default localhost)   MUN_DB_PORT (default 3306)
//   MUN_DB_USER (default root)        MUN_DB_PASS (default empty)
//   MUN_DB_NAME (default mun_db)
#pragma once
#include "Committee.h"
#include <map>
#include <cstdlib>

// ---- MySQL C API headers (works with MSVC and Linux) ----
#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX                 // stop windows.h defining min/max macros (breaks std::numeric_limits<...>::max)
  #endif
  #include <winsock2.h>            // must come before windows.h
  #include <windows.h>
  #include <mysql.h>               // from "<MySQL install>\include"
  #ifdef _MSC_VER
    #pragma comment(lib, "libmysql.lib")   // auto-link; set Additional Library Directories to "<MySQL install>\lib"
  #endif
#else
  #include <mysql/mysql.h>
#endif

static MYSQL* conn = NULL;
static bool dbReady = false;

static string envOr(const char* key, const string& def) {
#ifdef _MSC_VER
    // getenv() triggers warning C4996 in MSVC; _dupenv_s is the safe version
    char* buf = NULL;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, key) == 0 && buf != NULL) {
        string v(buf);
        free(buf);
        return v;
    }
    return def;
#else
    const char* v = getenv(key);
    return v ? string(v) : def;
#endif
}

// escape text before putting it inside an SQL string (prevents quotes breaking queries / SQL injection)
static string esc(const string& s) {
    if (!dbReady) return s;                  // memory-only mode: no connection to escape with
    string out(s.size() * 2 + 1, '\0');
    unsigned long n = mysql_real_escape_string(conn, &out[0], s.c_str(), (unsigned long)s.size());
    out.resize(n);
    return out;
}

// run a statement; returns true on success (also true in memory-only mode)
static bool runQuery(const string& q) {
    if (!dbReady) return true;
    if (mysql_query(conn, q.c_str())) {
        cout << "Database error: " << mysql_error(conn) << "\n";
        return false;
    }
    return true;
}

static bool fetch(const string& q, MYSQL_RES*& res) {
    if (mysql_query(conn, q.c_str())) {
        cout << "Database error: " << mysql_error(conn) << "\n";
        return false;
    }
    res = mysql_store_result(conn);
    return res != NULL;
}

static void dbDisconnect() {
    if (conn) mysql_close(conn);
    conn = NULL;
    dbReady = false;
}

// ---------- connect + create tables ----------
void dbConnect() {
    conn = mysql_init(NULL);
    string host = envOr("MUN_DB_HOST", "localhost");
    string user = envOr("MUN_DB_USER", "root");
    string pass = envOr("MUN_DB_PASS", "");
    string name = envOr("MUN_DB_NAME", "mun_db");
    unsigned int port = (unsigned int)atoi(envOr("MUN_DB_PORT", "3306").c_str());

    if (!conn || !mysql_real_connect(conn, host.c_str(), user.c_str(), pass.c_str(),
                                     name.c_str(), port, NULL, 0)) {
        cout << "Could not connect to MySQL: " << (conn ? mysql_error(conn) : "init failed") << "\n"
             << "Running in MEMORY-ONLY mode (committee data will NOT be saved).\n";
        if (conn) mysql_close(conn);
        conn = NULL;
        dbReady = false;
        return;
    }
    dbReady = true;
    mysql_set_character_set(conn, "utf8mb4");
    atexit(dbDisconnect);

    runQuery("CREATE TABLE IF NOT EXISTS committees ("
             "id INT AUTO_INCREMENT PRIMARY KEY, "
             "name VARCHAR(100) NOT NULL UNIQUE)");
    runQuery("CREATE TABLE IF NOT EXISTS portfolios ("
             "id INT AUTO_INCREMENT PRIMARY KEY, "
             "committee_id INT NOT NULL, "
             "name VARCHAR(100) NOT NULL, "
             "capacity INT NOT NULL, "
             "UNIQUE KEY uq_committee_portfolio (committee_id, name), "
             "FOREIGN KEY (committee_id) REFERENCES committees(id) "
             "ON DELETE CASCADE ON UPDATE CASCADE)");
    runQuery("CREATE TABLE IF NOT EXISTS allocations ("
             "id INT AUTO_INCREMENT PRIMARY KEY, "
             "portfolio_id INT NOT NULL, "
             "delegate_name VARCHAR(100) NOT NULL, "
             "UNIQUE KEY uq_portfolio_delegate (portfolio_id, delegate_name), "
             "FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) "
             "ON DELETE CASCADE)");
    cout << "Connected to MySQL database '" << name << "'.\n";
}

// ---------- load everything into memory ----------
void dbLoad() {
    if (!dbReady) return;
    committees.clear();

    map<int, int> cIndex;                    // committee id -> index in `committees`
    map<int, pair<int, int> > pIndex;        // portfolio id -> (committee idx, portfolio idx)
    MYSQL_RES* r;
    MYSQL_ROW row;

    if (fetch("SELECT id, name FROM committees ORDER BY id", r)) {
        while ((row = mysql_fetch_row(r))) {
            cIndex[atoi(row[0])] = (int)committees.size();
            committees.push_back(Committee(row[1]));
        }
        mysql_free_result(r);
    }

    if (fetch("SELECT id, committee_id, name, capacity FROM portfolios ORDER BY id", r)) {
        while ((row = mysql_fetch_row(r))) {
            map<int, int>::iterator it = cIndex.find(atoi(row[1]));
            if (it == cIndex.end()) continue;
            Committee& c = committees[it->second];
            pIndex[atoi(row[0])] = make_pair(it->second, (int)c.portfolios.size());
            c.portfolios.push_back(Portfolio(row[2], atoi(row[3])));
        }
        mysql_free_result(r);
    }

    if (fetch("SELECT portfolio_id, delegate_name FROM allocations ORDER BY id", r)) {
        while ((row = mysql_fetch_row(r))) {
            map<int, pair<int, int> >::iterator it = pIndex.find(atoi(row[0]));
            if (it == pIndex.end()) continue;
            committees[it->second.first].portfolios[it->second.second].allocated.push_back(row[1]);
        }
        mysql_free_result(r);
    }
    cout << "Loaded " << committees.size() << " committee(s) from the database.\n";
}

// ---------- write-through operations (names are the stored names) ----------
bool dbAddCommittee(const string& n) {
    return runQuery("INSERT INTO committees(name) VALUES('" + esc(n) + "')");
}

bool dbRenameCommittee(const string& oldName, const string& newName) {
    return runQuery("UPDATE committees SET name='" + esc(newName) +
                    "' WHERE name='" + esc(oldName) + "'");
}

bool dbDeleteCommittee(const string& n) {
    return runQuery("DELETE FROM committees WHERE name='" + esc(n) + "'");
}

bool dbAddPortfolio(const string& c, const string& p, int cap) {
    return runQuery("INSERT INTO portfolios(committee_id, name, capacity) "
                    "SELECT id, '" + esc(p) + "', " + to_string(cap) +
                    " FROM committees WHERE name='" + esc(c) + "'");
}

bool dbUpdatePortfolio(const string& c, const string& oldP, const string& newP, int cap) {
    return runQuery("UPDATE portfolios SET name='" + esc(newP) + "', capacity=" + to_string(cap) +
                    " WHERE name='" + esc(oldP) + "' AND committee_id="
                    "(SELECT id FROM committees WHERE name='" + esc(c) + "')");
}

bool dbSetCapacity(const string& c, const string& p, int cap) {
    return runQuery("UPDATE portfolios SET capacity=" + to_string(cap) +
                    " WHERE name='" + esc(p) + "' AND committee_id="
                    "(SELECT id FROM committees WHERE name='" + esc(c) + "')");
}

bool dbDeletePortfolio(const string& c, const string& p) {
    return runQuery("DELETE FROM portfolios WHERE name='" + esc(p) + "' AND committee_id="
                    "(SELECT id FROM committees WHERE name='" + esc(c) + "')");
}

bool dbAllocate(const string& c, const string& p, const string& d) {
    return runQuery("INSERT INTO allocations(portfolio_id, delegate_name) "
                    "SELECT id, '" + esc(d) + "' FROM portfolios WHERE name='" + esc(p) +
                    "' AND committee_id=(SELECT id FROM committees WHERE name='" + esc(c) + "')");
}
